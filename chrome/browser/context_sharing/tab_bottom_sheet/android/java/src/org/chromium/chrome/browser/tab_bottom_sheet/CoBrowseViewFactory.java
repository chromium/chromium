// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;

import org.jni_zero.CalledByNative;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetFusebox.TabBottomSheetFuseboxConfig;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Factory for creating co-browse content. */
@NullMarked
public class CoBrowseViewFactory {

    private final Activity mActivity;
    private final TabBottomSheetFuseboxConfig mFuseboxConfig;
    private final WindowAndroid mWindowAndroid;
    private final NonNullObservableSupplier<Profile> mProfileSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final SnackbarManager mSnackbarManager;

    /**
     * Factory responsible for creating co-browse content.
     *
     * @param activity The current {@link Activity} instance.
     * @param fuseboxConfig The configuration for the fusebox.
     * @param profileSupplier A supplier for the current {@link Profile}.
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for managing activity
     *     lifecycle.
     * @param snackbarManager The {@link SnackbarManager} for managing snackbar messages.
     */
    public CoBrowseViewFactory(
            Activity activity,
            TabBottomSheetFuseboxConfig fuseboxConfig,
            NonNullObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SnackbarManager snackbarManager) {
        mActivity = activity;
        mFuseboxConfig = fuseboxConfig;
        mProfileSupplier = profileSupplier;
        mWindowAndroid = windowAndroid;
        mLifecycleDispatcher = lifecycleDispatcher;
        mSnackbarManager = snackbarManager;

        TabBottomSheetUtils.attachFactoryToWindow(windowAndroid, this);
    }

    public void destroy() {
        TabBottomSheetUtils.detachFactoryFromWindow(mWindowAndroid);
    }

    /**
     * Builds the co-browse views.
     *
     * @param webContents The {@link WebContents} to be displayed in the thin web view.
     * @param showToolbar Whether to show the toolbar.
     * @param showFusebox Whether to show the fusebox.
     * @return The {@link CoBrowseViews} instance.
     */
    CoBrowseViews buildCoBrowseViews(
            WebContents webContents, boolean showToolbar, boolean showFusebox) {
        TabBottomSheetToolbar toolbar =
                showToolbar ? new TabBottomSheetSimpleToolbar(mActivity) : null;
        TabBottomSheetWebUi webUi = new TabBottomSheetWebUi(mActivity, mWindowAndroid);
        TabBottomSheetFusebox fusebox =
                showFusebox || TabBottomSheetUtils.shouldShowFusebox()
                        ? new TabBottomSheetFusebox(
                                mActivity,
                                mFuseboxConfig,
                                mProfileSupplier,
                                mWindowAndroid,
                                mLifecycleDispatcher,
                                CallbackUtils.emptyCallback(),
                                mSnackbarManager)
                        : null;

        webUi.setWebContents(webContents);

        return new CoBrowseViews(mActivity, toolbar, webUi, fusebox);
    }

    @CalledByNative
    public static @Nullable CoBrowseViews getCoBrowseViews(
            WindowAndroid windowAndroid, WebContents webContents) {
        CoBrowseViewFactory factory = TabBottomSheetUtils.getFactoryFromWindow(windowAndroid);
        if (factory == null) {
            return null;
        }
        return factory.buildCoBrowseViews(
                webContents, /* showToolbar= */ false, /* showFusebox= */ false);
    }
}
