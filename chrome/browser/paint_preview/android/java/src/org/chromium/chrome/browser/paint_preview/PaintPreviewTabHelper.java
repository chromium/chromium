// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Manages the Paint Preview component for a given {@link Tab}. Destroyed together with the tab.
 */
public class PaintPreviewTabHelper extends EmptyTabObserver implements UserData {
    public static final Class<PaintPreviewTabHelper> USER_DATA_KEY = PaintPreviewTabHelper.class;

    private Tab mTab;
    private PaintPreviewDemoManager mPaintPreviewDemoManager;

    public static void createForTab(Tab tab) {
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new PaintPreviewTabHelper(tab));
    }

    public static PaintPreviewTabHelper get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private PaintPreviewTabHelper(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
        mPaintPreviewDemoManager = new PaintPreviewDemoManager(tab);
    }

    /**
     * Attempts to capture the current tab as a Paint Preview and displays it. This is only
     * accessible from a menu item that is guarded behind an about:flag.
     */
    public void showPaintPreviewDemo() {
        if (mTab == null || !qualifiesForCapture(mTab)) {
            return;
        }

        PaintPreviewCompositorUtils.warmupCompositor();
        mPaintPreviewDemoManager.showPaintPreviewDemo();
    }

    /**
     * Removes the Paint Preview demo view if it's being displayed. Paint Preview demo is only
     * accessible from a menu item that is guarded behind an about:flag.
     * @return Whether the Paint Preview demo was showing.
     */
    public boolean removePaintPreviewDemoIfShowing() {
        if (mPaintPreviewDemoManager.isShowingPaintPreviewDemo()) {
            mPaintPreviewDemoManager.removePaintPreviewDemo();
            return true;
        }

        return false;
    }

    @Override
    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
        // Note this doesn't apply to forced reloads, which is desirable in this case.
        if (!toDifferentDocument) return;

        removePaintPreviewDemoIfShowing();
    }

    @Override
    public void onDestroyed(Tab tab) {
        assert mTab == tab;
        tab.removeObserver(this);
        mTab = null;
    }

    @Override
    public void destroy() {
        mPaintPreviewDemoManager.destroy();
    }

    /**
     * Checks whether a given {@link Tab} qualifies for Paint Preview capture.
     */
    private boolean qualifiesForCapture(Tab tab) {
        return !tab.isIncognito() && !tab.isNativePage() && !tab.isShowingErrorPage()
                && tab.getWebContents() != null;
    }
}
