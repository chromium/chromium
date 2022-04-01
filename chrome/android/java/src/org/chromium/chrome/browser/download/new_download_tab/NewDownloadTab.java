// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.new_download_tab;

import static org.chromium.chrome.browser.tab.TabViewProvider.Type.NEW_DOWNLOAD_TAB;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialCoordinator;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialCoordinatorFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.ui.base.WindowAndroid;

/** Represents the page shown when a CCT is created to download a file. */
public class NewDownloadTab extends EmptyTabObserver implements UserData, TabViewProvider {
    private static final Class<NewDownloadTab> USER_DATA_KEY = NewDownloadTab.class;

    private final Tab mTab;
    private final DownloadInterstitialCoordinator mCoordinator;

    /**
     * Checks if a NewDownloadTab exists for the given tab and creates one if not.
     * @param tab The parent tab to contain the NewDownloadTab.
     * @return The NewDownloadTab attached to the parent tab if present or a new instance of
     *         NewDownloadTab attached to the parent tab otherwise.
     */
    public static NewDownloadTab from(Tab tab) {
        assert tab.isInitialized();
        NewDownloadTab newDownloadTab = get(tab);
        if (newDownloadTab == null) {
            newDownloadTab =
                    tab.getUserDataHost().setUserData(USER_DATA_KEY, new NewDownloadTab(tab));
        }
        return newDownloadTab;
    }

    /**
     * Returns the instance of NewDownloadTab attached to a given tab.
     * @param tab The parent tab containing the NewDownloadTab.
     * @return The NewDownloadTab attached to the parent tab.
     */
    private static NewDownloadTab get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /** Displays the NewDownloadTab inside the parent tab. */
    public void show() {
        mTab.addObserver(this);
        if (!isViewAttached()) {
            attachView();
        }
    }

    /** Removes the NewDownloadTab instance from its parent. */
    public void removeIfPresent() {
        mTab.getTabViewManager().removeTabViewProvider(this);
    }

    // TabObserver implementation.
    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (window == null) {
            removeIfPresent();
        } else {
            attachView();
        }
    }

    // UserData implementation.
    @Override
    public void destroy() {
        mTab.removeObserver(this);
        mTab.getTabViewManager().removeTabViewProvider(this);
        mCoordinator.destroy();
    }

    @Override
    public int getTabViewProviderType() {
        return NEW_DOWNLOAD_TAB;
    }

    @Override
    public View getView() {
        return mCoordinator.getView();
    }

    @Override
    public void onHidden() {
        removeIfPresent();
    }

    private NewDownloadTab(Tab tab) {
        mTab = tab;
        mCoordinator = DownloadInterstitialCoordinatorFactory.create(
                tab.getContext(), tab.getOriginalUrl().getSpec(), tab.getWindowAndroid());
    }

    private boolean isViewAttached() {
        return mCoordinator.getView() != null && mTab.getTabViewManager().isShowing(this);
    }

    private void attachView() {
        mTab.getTabViewManager().addTabViewProvider(this);
    }
}