// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.new_download_tab;
import static org.chromium.chrome.browser.tab.TabViewProvider.Type.NEW_DOWNLOAD_TAB;

import android.content.Context;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Represents the page shown when a download is initiated from Chrome/CCT.
 */
@RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
public class NewDownloadTab extends EmptyTabObserver implements UserData, TabViewProvider {
    private static final Class<NewDownloadTab> USER_DATA_KEY = NewDownloadTab.class;
    private final Tab mTab;
    private View mView;

    /** Creates a new instance of NewDownloadTab and attaches it to a tab. */
    public static NewDownloadTab from(Tab tab) {
        assert tab.isInitialized();
        NewDownloadTab newDownloadTab = get(tab);
        if (newDownloadTab == null) {
            newDownloadTab =
                    tab.getUserDataHost().setUserData(USER_DATA_KEY, new NewDownloadTab(tab));
        }
        return newDownloadTab;
    }

    /** Returns the instance of NewDownloadTab currently attached to a given tab. */
    public static NewDownloadTab get(Tab tab) {
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
        mView = null;
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
    }

    @Override
    public int getTabViewProviderType() {
        return NEW_DOWNLOAD_TAB;
    }

    @Override
    public View getView() {
        return mView;
    }

    @VisibleForTesting
    boolean isViewAttached() {
        return mView != null && mTab.getTabViewManager().isShowing(this);
    }

    private NewDownloadTab(Tab tab) {
        mTab = tab;
    }

    private View createView() {
        Context context = mTab.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        View newDownloadTabView = inflater.inflate(R.layout.new_download_tab, null);
        newDownloadTabView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        return newDownloadTabView;
    }

    private void attachView() {
        assert mView == null;
        mView = createView();
        mTab.getTabViewManager().addTabViewProvider(this);
    }
}
