// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.bookmarks.AddToBookmarksToolbarButtonController;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.OptionalNewTabButtonController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/** Implements tabbed browser-specific behavior of adaptive toolbar button. */
public class TabbedAdaptiveToolbarBehavior implements AdaptiveToolbarBehavior {
    private final Context mContext;
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<TabCreatorManager> mTabCreatorManagerSupplier;
    private final Runnable mRegisterVoiceSearchRunnable;

    public TabbedAdaptiveToolbarBehavior(
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<TabCreatorManager> tabCreatorManagerSupplier,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            ActivityTabProvider activityTabProvider,
            Runnable registerVoiceSearchRunnable) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mActivityTabProvider = activityTabProvider;
        mRegisterVoiceSearchRunnable = registerVoiceSearchRunnable;
    }

    @Override
    public void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller, Supplier<Tracker> trackerSupplier) {
        var newTabButton =
                new OptionalNewTabButtonController(
                        mContext,
                        AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon),
                        mActivityLifecycleDispatcher,
                        mTabCreatorManagerSupplier,
                        mActivityTabProvider,
                        trackerSupplier);
        controller.addButtonVariant(AdaptiveToolbarButtonVariant.NEW_TAB, newTabButton);
        var addToBookmarks =
                new AddToBookmarksToolbarButtonController(
                        mActivityTabProvider,
                        mContext,
                        mActivityLifecycleDispatcher,
                        mTabBookmarkerSupplier,
                        trackerSupplier,
                        mBookmarkModelSupplier);
        controller.addButtonVariant(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS, addToBookmarks);

        mRegisterVoiceSearchRunnable.run();
    }

    @Override
    public int resultFilter(List<Integer> segmentationResults) {
        return AdaptiveToolbarBehavior.defaultResultFilter(mContext, segmentationResults);
    }

    @Override
    public boolean canShowManualOverride(int manualOverride) {
        return true;
    }

    @Override
    public boolean useRawResults() {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    @Override
    public @AdaptiveToolbarButtonVariant int getSegmentationDefault() {
        return AdaptiveToolbarFeatures.getDefaultButtonVariant(mContext);
    }
}
