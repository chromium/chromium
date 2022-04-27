// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/** Coordinator of the single tab tab switcher. */
class SingleTabSwitcherCoordinator implements TabSwitcher {
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final SingleTabSwitcherMediator mMediator;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabSwitcher.TabListDelegate mTabListDelegate;

    SingleTabSwitcherCoordinator(@NonNull Activity activity, @NonNull ViewGroup container,
            @NonNull TabModelSelector tabModelSelector) {
        PropertyModel propertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        SingleTabView singleTabView = (SingleTabView) LayoutInflater.from(activity).inflate(
                R.layout.single_tab_view_layout, container, false);
        container.addView(singleTabView);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                propertyModel, singleTabView, SingleTabViewBinder::bind);
        mTabListFaviconProvider = new TabListFaviconProvider(activity, false);
        mMediator = new SingleTabSwitcherMediator(
                activity, propertyModel, tabModelSelector, mTabListFaviconProvider);
        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)) {
            new TabAttributeCache(tabModelSelector);
        }

        // Most of these interfaces should be unused. They are invalid implementations.
        mTabListDelegate = new TabSwitcher.TabListDelegate() {
            @Override
            public int getResourceId() {
                return 0;
            }

            @Override
            public long getLastDirtyTime() {
                assert false : "should not reach here";
                return 0;
            }

            @Override
            @VisibleForTesting
            public void setBitmapCallbackForTesting(Callback<Bitmap> callback) {
                assert false : "should not reach here";
            }

            @Override
            @VisibleForTesting
            public int getBitmapFetchCountForTesting() {
                assert false : "should not reach here";
                return 0;
            }

            @Override
            @VisibleForTesting
            public int getSoftCleanupDelayForTesting() {
                assert false : "should not reach here";
                return 0;
            }

            @Override
            @VisibleForTesting
            public int getCleanupDelayForTesting() {
                assert false : "should not reach here";
                return 0;
            }

            @Override
            @VisibleForTesting
            public int getTabListTopOffset() {
                return 0;
            }

            @Override
            @VisibleForTesting
            public int getListModeForTesting() {
                assert false : "should not reach here";
                return 0;
            }

            @Override
            public boolean prepareOverview() {
                return true;
            }

            @Override
            public void postHiding() {}

            @Override
            public Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate) {
                assert false : "should not reach here";
                return null;
            }
        };
    }

    // TabSwitcher implementation.
    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        mMediator.setOnTabSelectingListener(listener);
    }

    @Override
    public void initWithNative(Context context, TabContentManager tabContentManager,
            DynamicResourceLoader dynamicResourceLoader, SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager) {
        mTabListFaviconProvider.initWithNative(Profile.getLastUsedRegularProfile());
        mMediator.initWithNative();
    }

    @Override
    public Controller getController() {
        return mMediator;
    }

    @Override
    public TabListDelegate getTabListDelegate() {
        return mTabListDelegate;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        assert false : "should not reach here";
        return null;
    }
}
