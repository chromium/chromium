// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * A coordinator for TabGroupUi component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of
 * shared component objects.
 */
public class TabGroupUiCoordinator implements TabGroupUiMediator.ResetHandler, TabGroupUi,
                                              PauseResumeWithNativeObserver,
                                              TabGroupUiMediator.TabGroupUiController {
    static final String COMPONENT_NAME = "TabStrip";
    private final Context mContext;
    private final PropertyModel mModel;
    private final ThemeColorProvider mThemeColorProvider;
    private final TabGroupUiToolbarView mToolbarView;
    private final ViewGroup mTabListContainerView;
    private final ScrimCoordinator mScrimCoordinator;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private TabGridDialogCoordinator mTabGridDialogCoordinator;
    private TabListCoordinator mTabStripCoordinator;
    private TabGroupUiMediator mMediator;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private ChromeActivity mActivity;

    /**
     * Creates a new {@link TabGroupUiCoordinator}
     */
    public TabGroupUiCoordinator(ViewGroup parentView, ThemeColorProvider themeColorProvider,
            ScrimCoordinator scrimCoordinator,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mContext = parentView.getContext();
        mThemeColorProvider = themeColorProvider;
        mScrimCoordinator = scrimCoordinator;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);
        mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(mContext).inflate(
                R.layout.bottom_tab_strip_toolbar, parentView, false);
        mTabListContainerView = mToolbarView.getViewContainer();
        parentView.addView(mToolbarView);
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    @Override
    public void initializeWithNative(Activity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController) {
        if (UmaSessionStats.isMetricsServiceAvailable()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GROUPS_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }
        assert activity instanceof ChromeTabbedActivity;
        mActivity = (ChromeActivity) activity;
        TabModelSelector tabModelSelector = mActivity.getTabModelSelector();
        TabContentManager tabContentManager = mActivity.getTabContentManager();

        boolean actionOnAllRelatedTabs = TabUiFeatureUtilities.isConditionalTabStripEnabled();
        mTabStripCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.STRIP,
                mContext, tabModelSelector, null, null, actionOnAllRelatedTabs, null, null,
                TabProperties.UiType.STRIP, null, null, mTabListContainerView, true,
                COMPONENT_NAME);
        mTabStripCoordinator.initWithNative(
                mActivity.getCompositorViewHolder().getDynamicResourceLoader());

        mModelChangeProcessor = PropertyModelChangeProcessor.create(mModel,
                new TabGroupUiViewBinder.ViewHolder(
                        mToolbarView, mTabStripCoordinator.getContainerView()),
                TabGroupUiViewBinder::bind);

        // TODO(crbug.com/972217): find a way to enable interactions between grid tab switcher
        //  and the dialog here.
        TabGridDialogMediator.DialogController dialogController = null;
        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled() && mScrimCoordinator != null) {
            mTabGridDialogCoordinator = new TabGridDialogCoordinator(mContext, tabModelSelector,
                    tabContentManager, /* tabCreatorManager= */ mActivity,
                    activity.findViewById(R.id.coordinator), null, null, null,
                    mActivity.getShareDelegateSupplier(), mScrimCoordinator);
            mTabGridDialogCoordinator.initWithNative(mContext, tabModelSelector, tabContentManager,
                    mTabStripCoordinator.getTabGroupTitleEditor());
            dialogController = mTabGridDialogCoordinator.getDialogController();
        }

        mMediator = new TabGroupUiMediator(activity, visibilityController, this, mModel,
                tabModelSelector, /* tabCreatorManager= */ mActivity,
                mActivity.getOverviewModeBehaviorSupplier(), mThemeColorProvider, dialogController,
                mActivity.getLifecycleDispatcher(), /* snackbarManageable= */ mActivity,
                mOmniboxFocusStateSupplier);

        TabGroupUtils.startObservingForCreationIPH();

        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) return;

        mActivityLifecycleDispatcher = mActivity.getLifecycleDispatcher();
        mActivityLifecycleDispatcher.register(this);

        // TODO(meiliang): Potential leak if the observer is added after restoreCompleted. Fix it.
        // Record the group count after all tabs are being restored. This only happen once per life
        // cycle, therefore remove the observer after recording. We only focus on normal tab model
        // because we don't restore tabs in incognito tab model.
        tabModelSelector.getModel(false).addObserver(new TabModelObserver() {
            @Override
            public void restoreCompleted() {
                recordTabGroupCount();
                recordSessionCount();
                tabModelSelector.getModel(false).removeObserver(this);
            }
        });
    }

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return mTabGridDialogCoordinator::isVisible;
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator} to reset the tab strip.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(mActivity.getWindowAndroid());
        if (tabs != null && tabs.size() > 1
                && bottomSheetController.getSheetState()
                        == BottomSheetController.SheetState.HIDDEN) {
            TabGroupUtils.maybeShowIPH(FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE,
                    mTabStripCoordinator.getContainerView(),
                    TabUiFeatureUtilities.isLaunchBugFixEnabled() ? bottomSheetController : null);
        }
        mTabStripCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is expanded or the dialog is shown.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetGridWithListOfTabs(List<Tab> tabs) {
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.resetWithListOfTabs(tabs);
        }
    }

    /**
     * TabGroupUi implementation.
     */
    @Override
    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        // Early return if the component hasn't initialized yet.
        if (mActivity == null) return;

        mTabStripCoordinator.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mModelChangeProcessor.destroy();
        mMediator.destroy();
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
        }
    }

    // PauseResumeWithNativeObserver implementation.
    @Override
    public void onResumeWithNative() {
        // Since we use AsyncTask for restoring tabs, this method can be called before or after
        // restoring all tabs. Therefore, we skip recording the count here during cold start and
        // record that elsewhere when TabModel emits the restoreCompleted signal.
        if (!mActivity.isWarmOnResume()) return;

        recordTabGroupCount();
        recordSessionCount();
    }

    private void recordTabGroupCount() {
        TabModelFilterProvider provider =
                mActivity.getTabModelSelector().getTabModelFilterProvider();

        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            TabModelFilter normalTabModelFilter = provider.getTabModelFilter(false);

            if (!(normalTabModelFilter instanceof TabGroupModelFilter)) {
                String actualType = normalTabModelFilter == null
                        ? "null"
                        : normalTabModelFilter.getClass().getName();
                assert false
                    : "Please file bug, this is unexpected. Expected TabGroupModelFilter, but was "
                      + actualType;

                return;
            }
        }

        TabGroupModelFilter normalFilter = (TabGroupModelFilter) provider.getTabModelFilter(false);
        TabGroupModelFilter incognitoFilter =
                (TabGroupModelFilter) provider.getTabModelFilter(true);
        int groupCount = normalFilter.getTabGroupCount() + incognitoFilter.getTabGroupCount();
        RecordHistogram.recordCountHistogram("TabGroups.UserGroupCount", groupCount);
        if (TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            int namedGroupCount = 0;
            for (int i = 0; i < normalFilter.getTabGroupCount(); i++) {
                int rootId = CriticalPersistedTabData.from(normalFilter.getTabAt(i)).getRootId();
                if (TabGroupUtils.getTabGroupTitle(rootId) != null) {
                    namedGroupCount += 1;
                }
            }
            for (int i = 0; i < incognitoFilter.getTabGroupCount(); i++) {
                int rootId = CriticalPersistedTabData.from(incognitoFilter.getTabAt(i)).getRootId();
                if (TabGroupUtils.getTabGroupTitle(rootId) != null) {
                    namedGroupCount += 1;
                }
            }
            RecordHistogram.recordCountHistogram("TabGroups.UserNamedGroupCount", namedGroupCount);
        }
    }

    private void recordSessionCount() {
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            TabModelFilter normalTabModelFilter =
                    mActivity.getTabModelSelector().getTabModelFilterProvider().getTabModelFilter(
                            false);

            if (!(normalTabModelFilter instanceof TabGroupModelFilter)) {
                String actualType = normalTabModelFilter == null
                        ? "null"
                        : normalTabModelFilter.getClass().getName();
                assert false
                    : "Please file bug, this is unexpected. Expected TabGroupModelFilter, but was "
                      + actualType;

                return;
            }
        }

        OverviewModeBehavior overviewModeBehavior =
                (OverviewModeBehavior) mActivity.getOverviewModeBehaviorSupplier().get();

        if (overviewModeBehavior != null && overviewModeBehavior.overviewVisible()) {
            return;
        }

        Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
        if (currentTab == null) return;
        TabModelFilterProvider provider =
                mActivity.getTabModelSelector().getTabModelFilterProvider();
        ((TabGroupModelFilter) provider.getCurrentTabModelFilter()).recordSessionsCount(currentTab);
    }

    @Override
    public void onPauseWithNative() {}

    // TabGroupUiController implementation.
    @Override
    public void setupLeftButtonDrawable(int drawableId) {
        mMediator.setupLeftButtonDrawable(drawableId);
    }

    @Override
    public void setupLeftButtonOnClickListener(View.OnClickListener listener) {
        mMediator.setupLeftButtonOnClickListener(listener);
    }
}
