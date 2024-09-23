// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager.Observer;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;

/** Helper class attaching Tab's content layer to {@link TabContentManager}. */
public final class TabContentManagerHandler extends TabModelSelectorTabObserver {
    private final TabContentManager mTabContentManager;

    private final FullscreenManager mFullscreenManager;
    private final Observer mFullscreenObserver;

    // Indicates that thumbnail cache should be removed when tab becomes interactive.
    // Used when a request is made while a tab is not in interactive state so
    // the job should be done in a delayed manner.
    private boolean mShouldRemoveThumbnail;

    // A tab whose thumbnail needs to be removed.
    private Tab mThumbnailTab;

    public static void create(
            TabContentManager manager,
            FullscreenManager fullscreenManager,
            TabModelSelector selector) {
        new TabContentManagerHandler(manager, fullscreenManager, selector);
    }

    private TabContentManagerHandler(
            TabContentManager manager,
            FullscreenManager fullscreenManager,
            TabModelSelector selector) {
        super(selector);
        mTabContentManager = manager;
        mFullscreenManager = fullscreenManager;
        mFullscreenObserver =
                new Observer() {
                    @Override
                    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                        if (!tab.isUserInteractable()) {
                            mTabContentManager.removeTabThumbnail(tab.getId());
                        } else {
                            mThumbnailTab = tab;
                            mShouldRemoveThumbnail = true;
                        }
                    }
                };

        mFullscreenManager.addObserver(mFullscreenObserver);
    }

    @Override
    public void onInteractabilityChanged(Tab tab, boolean interactable) {
        if (interactable && mShouldRemoveThumbnail && mThumbnailTab != null) {
            mTabContentManager.removeTabThumbnail(mThumbnailTab.getId());
            mShouldRemoveThumbnail = false;
            mThumbnailTab = null;
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mFullscreenManager.removeObserver(mFullscreenObserver);
    }
}
