// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ai.AiAssistantService;
import org.chromium.chrome.browser.ai.PageSummaryButtonController;
import org.chromium.chrome.browser.bookmarks.AddToBookmarksToolbarButtonController;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonDataProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.OptionalNewTabButtonController;
import org.chromium.chrome.browser.toolbar.top.tab_strip.StripVisibilityState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.function.Supplier;

/** Implements tabbed browser-specific behavior of adaptive toolbar button. */
@NullMarked
public class TabbedAdaptiveToolbarBehavior implements AdaptiveToolbarBehavior {
    private final Context mContext;
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<@Nullable TabCreatorManager> mTabCreatorManagerSupplier;
    private final Runnable mRegisterVoiceSearchRunnable;
    private final Supplier<GroupSuggestionsButtonController>
            mGroupSuggestionsButtonControllerSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final ObservableSupplier<@StripVisibilityState Integer> mTabStripVisibilitySupplier;

    public TabbedAdaptiveToolbarBehavior(
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<@Nullable TabCreatorManager> tabCreatorManagerSupplier,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            ActivityTabProvider activityTabProvider,
            Runnable registerVoiceSearchRunnable,
            Supplier<GroupSuggestionsButtonController> groupSuggestionsButtonController,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            ObservableSupplier<@StripVisibilityState Integer> tabStripVisibilitySupplier) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mActivityTabProvider = activityTabProvider;
        mRegisterVoiceSearchRunnable = registerVoiceSearchRunnable;
        mGroupSuggestionsButtonControllerSupplier = groupSuggestionsButtonController;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mTabStripVisibilitySupplier = tabStripVisibilitySupplier;
    }

    @Override
    public void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller,
            Supplier<@Nullable Tracker> trackerSupplier) {
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
        var addToBookmarks =
                new AddToBookmarksToolbarButtonController(
                        mActivityTabProvider,
                        mContext,
                        mActivityLifecycleDispatcher,
                        mTabBookmarkerSupplier,
                        trackerSupplier,
                        mBookmarkModelSupplier);
        controller.addButtonVariant(AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS, addToBookmarks);
        var pageSummary =
                new PageSummaryButtonController(
                        mContext,
                        mModalDialogManagerSupplier.get(),
                        mActivityTabProvider,
                        AiAssistantService.getInstance(),
                        trackerSupplier);
        controller.addButtonVariant(AdaptiveToolbarButtonVariant.PAGE_SUMMARY, pageSummary);
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
