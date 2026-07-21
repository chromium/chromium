// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import static org.chromium.components.embedder_support.util.UrlConstants.CONTENT_SCHEME;

import android.app.Activity;
import android.net.Uri;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.Printable;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.ref.WeakReference;

/**
 * Wraps printing related functionality of a {@link Tab} object.
 *
 * <p>This class doesn't have any lifetime expectations with regards to Tab, since we keep a weak
 * reference.
 */
@JNINamespace("printing")
@NullMarked
public class TabPrinter implements Printable {
    private static final String TAG = "printing";

    private final WeakReference<Tab> mTab;
    private final @Nullable GlobalRenderFrameHostId mTargetFrameId;
    private final String mDefaultTitle;
    private final String mErrorMessage;
    private final boolean mPrintSelectionOnly;

    @CalledByNative
    private static TabPrinter getPrintable(Tab tab) {
        return new TabPrinter(tab);
    }

    /**
     * Triggers printing for the current selection in the specified tab.
     *
     * @param tab The tab to print.
     * @param rfh The render frame host containing the selection.
     */
    @CalledByNative
    public static void printSelection(Tab tab, @Nullable RenderFrameHost rfh) {
        ThreadUtils.assertOnUiThread();
        if (rfh == null) {
            // We cannot print the selection if we do not know which frame contains it.
            // Printing a fallback frame here could lead to printing the wrong page content.
            Log.w(TAG, "printSelection: no target frame; ignoring.");
            return;
        }
        WindowAndroid window = tab.getWindowAndroid();
        if (window == null) return;
        Activity activity = window.getActivity().get();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) return;
        PrintingController controller = PrintingControllerImpl.getInstance(window);
        if (controller == null || controller.isBusy()) return;

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;
        SelectionPopupController spc = SelectionPopupController.fromWebContents(webContents);
        if (spc == null || !spc.hasSelection()) return;

        // Final line of defense: re-check if printing is enabled by policy.
        Profile profile = Profile.fromWebContents(webContents);
        if (profile == null || !UserPrefs.get(profile).getBoolean(Pref.PRINTING_ENABLED)) {
            return;
        }

        spc.setPreserveSelectionOnNextLossOfFocus(true);
        spc.hidePopupsAndPreserveSelection();

        TabPrinter printer = new TabPrinter(tab, rfh.getGlobalRenderFrameHostId(), true);
        controller.startPrint(printer, new PrintManagerDelegateImpl(activity));
    }

    /**
     * Creates a {@link TabPrinter} for the given tab.
     *
     * @param tab The tab to print.
     */
    public TabPrinter(Tab tab) {
        this(tab, null, false);
    }

    /**
     * Creates a {@link TabPrinter} for the given tab with optional subframe targeting and selection
     * flags.
     *
     * @param tab The tab to print.
     * @param targetFrameId Optional target subframe ID to print. If specified, this overrides the
     *     default process and frame IDs passed into {@link #print(int, int)}.
     * @param printSelectionOnly Whether to print only the text selection within the frame.
     */
    public TabPrinter(
            Tab tab, @Nullable GlobalRenderFrameHostId targetFrameId, boolean printSelectionOnly) {
        mTab = new WeakReference<>(tab);
        mTargetFrameId = targetFrameId;
        mPrintSelectionOnly = printSelectionOnly;
        mDefaultTitle = ContextUtils.getApplicationContext().getString(R.string.menu_print);
        mErrorMessage =
                ContextUtils.getApplicationContext().getString(R.string.error_printing_failed);
    }

    @Override
    public boolean print(int renderProcessId, int renderFrameId) {
        if (!canPrint()) return false;
        Tab tab = mTab.get();
        assert tab != null && tab.isInitialized();
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;
        int targetProcessId = mTargetFrameId != null ? mTargetFrameId.childId() : renderProcessId;
        int targetFrameId =
                mTargetFrameId != null ? mTargetFrameId.frameRoutingId() : renderFrameId;
        return new WebContentsPrinter(webContents, mPrintSelectionOnly)
                .print(targetProcessId, targetFrameId);
    }

    @Override
    public String getTitle() {
        Tab tab = mTab.get();
        if (tab == null || !tab.isInitialized()) return mDefaultTitle;

        String title = tab.getTitle();
        if (!TextUtils.isEmpty(title)) return title;

        String url = tab.getUrl().getSpec();
        if (!TextUtils.isEmpty(url)) return url;

        return mDefaultTitle;
    }

    @Override
    public boolean canPrint() {
        Tab tab = mTab.get();
        if (tab == null || !tab.isInitialized()) {
            // Tab.isInitialized() will be false if tab is in destroy process.
            Log.d(TAG, "Tab is not available for printing.");
            return false;
        }

        if (tab.isHidden()) {
            // A tab is not printable if it is hidden, which prevents background tabs from
            // printing. However, some OS print UI flows can result in the Activity being stopped
            // and therefore the tab being considered 'hidden'. During the OS print UI flows, users
            // may invoke actions, like changing the print layout orientation, that cause this
            // method to be invoked while the Tab is hidden. To allow printing to continue in these
            // cases, we make an exception for the current tab.
            MonotonicObservableSupplier<TabModelSelector> supplier =
                    TabModelSelectorSupplier.from(tab.getWindowAndroid());
            TabModelSelector selector = (supplier != null) ? supplier.get() : null;
            if (selector == null || selector.getCurrentTab() != tab) {
                Log.d(
                        TAG,
                        "Tab is not available for printing because it is hidden and not the"
                                + " current tab.");
                return false;
            }
        }

        return true;
    }

    @Override
    public String getErrorMessage() {
        return mErrorMessage;
    }

    @Override
    public @Nullable InputStream getPdfInputStream() {
        Tab tab = mTab.get();
        if (tab == null || !tab.isInitialized()) {
            return null;
        }

        if (tab.isNativePage() && tab.getNativePage() instanceof PdfPage) {
            String pdfFilePath = tab.getNativePage().getCanonicalFilepath();
            if (pdfFilePath == null) {
                return null;
            }

            try {
                if (pdfFilePath.startsWith(CONTENT_SCHEME)) {
                    return ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .openInputStream(Uri.parse(pdfFilePath));
                } else {
                    File file = new File(pdfFilePath);
                    return new FileInputStream(file);
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to open PDF input stream.", e);
                return null;
            }
        } else {
            return null;
        }
    }
}
