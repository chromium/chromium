// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.bookmarks.AddToBookmarksToolbarButtonController;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.glic.GlicToolbarButtonController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonDataProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabstrip.StripVisibilityState;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.OptionalNewTabButtonController;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.function.Supplier;

/** Implements tabbed browser-specific behavior of adaptive toolbar button. */
@NullMarked
public class TabbedAdaptiveToolbarBehavior implements AdaptiveToolbarBehavior {
    private final Context mContext;
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Supplier<@Nullable TabBookmarker> mTabBookmarkerSupplier;
    private final NullableObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<@Nullable TabCreatorManager> mTabCreatorManagerSupplier;
    private final Runnable mRegisterVoiceSearchRunnable;
    private final Supplier<GroupSuggestionsButtonController>
            mGroupSuggestionsButtonControllerSupplier;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final MonotonicObservableSupplier<@StripVisibilityState Integer>
            mTabStripVisibilitySupplier;
    private final GlicToolbarButtonController.GlicButtonDelegate mToggleGlicCallback;
    private final Supplier<@Nullable ChromeAndroidTask> mChromeAndroidTaskSupplier;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    /**
     * @param context The Android context.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events.
     * @param tabCreatorManagerSupplier Used to create new tabs.
     * @param tabBookmarkerSupplier Used to bookmark tabs.
     * @param bookmarkModelSupplier Used to access bookmark data.
     * @param activityTabProvider Provider for the active tab.
     * @param registerVoiceSearchRunnable Runnable to register voice search.
     * @param groupSuggestionsButtonController Used to control group suggestions on the toolbar.
     * @param tabModelSelectorSupplier Used to access the current tab model.
     * @param tabStripVisibilitySupplier Used to check or observe tab strip visibility.
     * @param toggleGlicCallback Callback to toggle the Glic UI.
     * @param chromeAndroidTaskSupplier Supplier for the ChromeAndroidTask.
     * @param browserControlsVisibilityManager Manager for browser controls.
     */
    public TabbedAdaptiveToolbarBehavior(
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<@Nullable TabCreatorManager> tabCreatorManagerSupplier,
            Supplier<@Nullable TabBookmarker> tabBookmarkerSupplier,
            NullableObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            ActivityTabProvider activityTabProvider,
            Runnable registerVoiceSearchRunnable,
            Supplier<GroupSuggestionsButtonController> groupSuggestionsButtonController,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            MonotonicObservableSupplier<@StripVisibilityState Integer> tabStripVisibilitySupplier,
            GlicToolbarButtonController.GlicButtonDelegate toggleGlicCallback,
            Supplier<@Nullable ChromeAndroidTask> chromeAndroidTaskSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mActivityTabProvider = activityTabProvider;
        mRegisterVoiceSearchRunnable = registerVoiceSearchRunnable;
        mGroupSuggestionsButtonControllerSupplier = groupSuggestionsButtonController;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mTabStripVisibilitySupplier = tabStripVisibilitySupplier;
        mToggleGlicCallback = toggleGlicCallback;
        mChromeAndroidTaskSupplier = chromeAndroidTaskSupplier;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
    }

    @Override
    public void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller,
            Supplier<@Nullable Tracker> trackerSupplier) {
        if (!BottomBarConfigUtils.isBottomBarEnabled(mContext)) {
            var newTabButton =
                    new OptionalNewTabButtonController(
                            mContext,
                            AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon),
                            mActivityLifecycleDispatcher,
                            mTabCreatorManagerSupplier,
                            mActivityTabProvider,
                            trackerSupplier,
                            mTabStripVisibilitySupplier);
            controller.addButtonVariant(AdaptiveToolbarButtonVariant.NEW_TAB, newTabButton);
        }
        var addToBookmarks =
                new AddToBookmarksToolbarButtonController(
                        mActivityTabProvider.asObservable(),
                        mContext,
                        mActivityLifecycleDispatcher,
                        mTabBookmarkerSupplier,
                        trackerSupplier,
                        mBookmarkModelSupplier);
        controller.addButtonVariant(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS, addToBookmarks);
        if (AdaptiveToolbarFeatures.isTabGroupingPageActionEnabled()) {
            var tabGrouping =
                    new GroupSuggestionsButtonDataProvider(
                            mActivityTabProvider,
                            mContext,
                            AppCompatResources.getDrawable(mContext, R.drawable.ic_widgets),
                            mGroupSuggestionsButtonControllerSupplier,
                            mTabModelSelectorSupplier);
            controller.addButtonVariant(AdaptiveToolbarButtonVariant.TAB_GROUPING, tabGrouping);
        }

        if (!BottomBarConfigUtils.isBottomBarEnabled(mContext)
                && AdaptiveToolbarFeatures.isGlicActionEnabled()) {
            controller.addButtonVariant(
                    AdaptiveToolbarButtonVariant.GLIC,
                    new GlicToolbarButtonController(
                            mContext,
                            mActivityTabProvider,
                            mToggleGlicCallback,
                            trackerSupplier,
                            mChromeAndroidTaskSupplier,
                            mBrowserControlsVisibilityManager,
                            mTabModelSelectorSupplier));
        }

        mRegisterVoiceSearchRunnable.run();
    }

    @Override
    public int resultFilter(List<Integer> segmentationResults) {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector != null) {
            Profile profile = selector.getCurrentModel().getProfile();
            if (profile != null
                    && AdaptiveToolbarFeatures.shouldForciblyShowGlicButton(mContext, profile)) {
                return AdaptiveToolbarButtonVariant.GLIC;
            }
        }

        return AdaptiveToolbarBehavior.defaultResultFilter(mContext, segmentationResults);
    }

    @Override
    public boolean canShowManualOverride(int manualOverride) {
        // Ignore manual overrides for GLIC and New Tab when the Android Bottom Bar is enabled as
        // these buttons have a dedicated spot in the bottom bar.
        if (BottomBarConfigUtils.isBottomBarEnabled(mContext)
                && (manualOverride == AdaptiveToolbarButtonVariant.GLIC
                        || manualOverride == AdaptiveToolbarButtonVariant.NEW_TAB)) {
            return false;
        }
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
