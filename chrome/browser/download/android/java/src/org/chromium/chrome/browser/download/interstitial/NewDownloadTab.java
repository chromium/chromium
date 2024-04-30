// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.chromium.chrome.browser.tab.TabViewProvider.Type.NEW_DOWNLOAD_TAB;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Represents the page shown when a CCT is created to download a file. */
public class NewDownloadTab extends EmptyTabObserver
        implements UserData, UnownedUserData, TabViewProvider {
    private static final Class<NewDownloadTab> USER_DATA_KEY = NewDownloadTab.class;

    private final Tab mTab;
    private final Activity mActivity;
    private final DownloadInterstitialCoordinator mCoordinator;

    /**
     * Checks if a NewDownloadTab exists for the given tab and creates one if not.
     * @param tab The parent tab to contain the NewDownloadTab.
     * @return The NewDownloadTab attached to the parent tab if present or a new instance of
     *         NewDownloadTab attached to the parent tab otherwise.
     */
    public static NewDownloadTab from(
            Tab tab, DownloadInterstitialCoordinator coordinator, Activity activity) {
        assert tab.isInitialized();
        NewDownloadTab newDownloadTab = get(tab);
        if (newDownloadTab == null) {
            newDownloadTab =
                    tab.getUserDataHost()
                            .setUserData(
                                    USER_DATA_KEY, new NewDownloadTab(tab, coordinator, activity));
        }
        return newDownloadTab;
    }

    /**
     * Identify the {@link NewDownloadTab} instance attached to the window android using
     * {@link UnownedUserData} through the {@link NewDownloadTabProvider} and finish its parent
     * activity.
     * @param windowAndroid The window android containing the new download tab.
     */
    public static void closeExistingNewDownloadTab(WindowAndroid windowAndroid) {
        NewDownloadTab instance = NewDownloadTabProvider.from(windowAndroid);
        if (instance != null) {
            instance.onDownloadDialogCancelled();
        }
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

    /**
     * Called when either a duplicate download or a download later dialog are dismissed.
     * Destroys the NewDownloadTab and finishes the parent activity.
     */
    public void onDownloadDialogCancelled() {
        destroy();
        mActivity.finish();
    }

    /** Removes the NewDownloadTab instance from its parent. */
    public void removeIfPresent() {
        mTab.getTabViewManager().removeTabViewProvider(this);
    }

    // TabObserver implementation.
    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        if (TextUtils.equals(url.getHost(), UrlConstants.RECENT_TABS_HOST)) {
            destroy();
        }
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (window == null) {
            removeIfPresent();
            return;
        }
        attachView();
        mCoordinator.onTabReparented(tab.getContext());
    }

    // UserData implementation.
    @Override
    public void destroy() {
        mTab.removeObserver(this);
        mTab.getTabViewManager().removeTabViewProvider(this);
        mCoordinator.destroy();
        NewDownloadTabProvider.detach(this);
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
    public @ColorInt int getBackgroundColor(Context context) {
        return SemanticColorUtils.getDefaultBgColor(context);
    }

    @Override
    public void onHidden() {
        removeIfPresent();
    }

    private NewDownloadTab(
            Tab tab, DownloadInterstitialCoordinator coordinator, Activity activity) {
        mTab = tab;
        mActivity = activity;
        mCoordinator = coordinator;
        NewDownloadTabProvider.attach(tab.getWindowAndroid(), this);
    }

    private boolean isViewAttached() {
        return mCoordinator.getView() != null && mTab.getTabViewManager().isShowing(this);
    }

    private void attachView() {
        mTab.getTabViewManager().addTabViewProvider(this);
    }
}
