// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.Predicate;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * The back press handler as the final step of back press handling. This is always enabled in order
 * to manually minimize app and close tab if necessary.
 */
public class MinimizeAppAndCloseTabBackPressHandler implements BackPressHandler {
    static final String HISTOGRAM = "Android.BackPress.MinimizeAppAndCloseTab";

    // An always-enabled supplier since this handler is the final step of back press handling.
    private final ObservableSupplierImpl<Boolean> mBackPressSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Callback<TabModelSelector> mOnTabModelSelectorAvailableCallback;
    private final Predicate<Tab> mBackShouldCloseTab;
    private final Callback<Tab> mSendToBackground;

    @IntDef({MinimizeAppAndCloseTabType.MINIMIZE_APP, MinimizeAppAndCloseTabType.CLOSE_TAB,
            MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB,
            MinimizeAppAndCloseTabType.NUM_TYPES})
    public @interface MinimizeAppAndCloseTabType {
        int MINIMIZE_APP = 0;
        int CLOSE_TAB = 1;
        int MINIMIZE_APP_AND_CLOSE_TAB = 2;
        int NUM_TYPES = 3;
    }

    /**
     * Record metrics of how back press is finally consumed by the app.
     * @param type The action we do when back press is consumed.
     */
    public static void record(@MinimizeAppAndCloseTabType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM, type, MinimizeAppAndCloseTabType.NUM_TYPES);
    }

    public MinimizeAppAndCloseTabBackPressHandler(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            Predicate<Tab> backShouldCloseTab, Callback<Tab> sendToBackground) {
        mBackShouldCloseTab = backShouldCloseTab;
        mSendToBackground = sendToBackground;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mOnTabModelSelectorAvailableCallback = this::onTabModelSelectorAvailable;
        tabModelSelectorSupplier.addObserver(mOnTabModelSelectorAvailableCallback);
        mBackPressSupplier.set(true);
    }

    @Override
    public void handleBackPress() {
        if (mTabModelSelectorSupplier.get() == null) {
            mSendToBackground.onResult(null);
            return;
        }
        Tab currentTab = mTabModelSelectorSupplier.get().getCurrentTab();
        // At this point we know either the tab will close or the app will minimize.
        NativePage nativePage = currentTab.getNativePage();
        if (nativePage != null) {
            nativePage.notifyHidingWithBack();
        }

        final boolean shouldCloseTab = mBackShouldCloseTab.test(currentTab);

        // Minimize the app if either:
        // - we decided not to close the tab
        // - we decided to close the tab, but it was opened by an external app, so we will go
        //   exit Chrome on top of closing the tab
        final boolean minimizeApp =
                !shouldCloseTab || TabAssociatedApp.isOpenedFromExternalApp(currentTab);
        if (minimizeApp) {
            record(shouldCloseTab ? MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB
                                  : MinimizeAppAndCloseTabType.MINIMIZE_APP);
            mSendToBackground.onResult(shouldCloseTab ? currentTab : null);
        } else { // shouldCloseTab is always true if minimizeApp is false.
            record(MinimizeAppAndCloseTabType.CLOSE_TAB);
            WebContents webContents = currentTab.getWebContents();
            if (webContents != null) webContents.dispatchBeforeUnload(false);
        }
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressSupplier;
    }

    private void onTabModelSelectorAvailable(TabModelSelector tabModelSelector) {
        mTabModelSelectorSupplier.removeObserver(mOnTabModelSelectorAvailableCallback);
    }
}
