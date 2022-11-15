// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.UNDO_DISMISS_SNACKBAR_DURATION;

import android.content.Context;
import android.os.Handler;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.layouts.FilterLayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.FeatureStatus;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.ReasonToShow;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_groups.EmptyTabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * A mediator for the TabGroupUi. Responsible for managing the internal state of the component.
 */
public class TabGroupUiMediator implements SnackbarManager.SnackbarController, BackPressHandler {
    /**
     * An interface to control the TabGroupUi component.
     */
    interface TabGroupUiController {
        /**
         * Setup the drawable in TabGroupUi left button with a drawable ID.
         * @param drawableId Resource ID of the drawable to setup the left button.
         */
        void setupLeftButtonDrawable(int drawableId);

        /**
         * Setup the {@link View.OnClickListener} of the left button in TabGroupUi.
         * @param listener {@link View.OnClickListener} to setup the left button.
         */
        void setupLeftButtonOnClickListener(View.OnClickListener listener);
    }

    /**
     * Defines an interface for a {@link TabGroupUiMediator} reset event
     * handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabGroupUiMediator}
         * when the bottom sheet is collapsed or the dialog is hidden.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetStripWithListOfTabs(List<Tab> tabs);

        /**
         * Handles a reset event originated from {@link TabGroupUiMediator}
         * when the bottom sheet is expanded or the dialog is shown.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetGridWithListOfTabs(List<Tab> tabs);
    }

    private final Context mContext;
    private final PropertyModel mModel;
    private final TabModelObserver mTabModelObserver;
    private final ResetHandler mResetHandler;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final BottomControlsCoordinator
            .BottomControlsVisibilityController mVisibilityController;
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final TabGridDialogMediator.DialogController mTabGridDialogController;
    private final IncognitoStateObserver mIncognitoStateObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final SnackbarManager mSnackbarManager;
    private final Snackbar mUndoClosureSnackBar;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;

    private CallbackController mCallbackController = new CallbackController();
    private final LayoutStateObserver mLayoutStateObserver;
    private LayoutStateProvider mLayoutStateProvider;

    private TabGroupModelFilter.Observer mTabGroupModelFilterObserver;
    private PauseResumeWithNativeObserver mPauseResumeWithNativeObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private Callback<Boolean> mOmniboxFocusObserver;
    private boolean mIsTabGroupUiVisible;
    private boolean mIsShowingOverViewMode;
    private boolean mActivatedButNotShown;

    TabGroupUiMediator(Context context,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            ResetHandler resetHandler, PropertyModel model, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            IncognitoStateProvider incognitoStateProvider,
            @Nullable TabGridDialogMediator.DialogController dialogController,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            SnackbarManager snackbarManager,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mContext = context;
        mResetHandler = resetHandler;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mVisibilityController = visibilityController;
        mIncognitoStateProvider = incognitoStateProvider;
        mTabGridDialogController = dialogController;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mSnackbarManager = snackbarManager;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mUndoClosureSnackBar =
                Snackbar.make(context.getString(R.string.undo_tab_strip_closure_message), this,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_CONDITIONAL_TAB_STRIP_DISMISS_UNDO)
                        .setAction(context.getString(R.string.undo), null)
                        .setDuration(UNDO_DISMISS_SNACKBAR_DURATION);

        if (layoutStateProviderSupplier.get() != null
                && layoutStateProviderSupplier.get().isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            // It is possible that the overview mode is showing when the TabGroupUiMediator is
            // created, sets the mIsShowingOverViewMode early to prevent the Tab strip is wrongly
            // showing on the Start surface homepage. See https://crbug.com/1239272.
            mIsShowingOverViewMode = true;
        }

        // register for tab model
        mTabModelObserver = new TabModelObserver() {
            private int mAddedTabId = Tab.INVALID_TAB_ID;
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (type == TabSelectionType.FROM_NEW) {
                    mAddedTabId = tab.getId();
                }
                if (lastId != tab.getId() && mSnackbarManager.isShowing()) {
                    // Dismiss undo snackbar when there is a selection of different tab.
                    mSnackbarManager.dismissSnackbars(TabGroupUiMediator.this);
                }
                // Maybe activate conditional tab strip for selection from toolbar swipe, but skip
                // the same tab selection that is probably due to partial toolbar swipe. Also, when
                // a new tab is created and selected, there will be two didSelectTab calls, one as
                // TabSelectionType.FROM_NEW and the other as TabSelectionType.FROM_USER. We skip
                // this kind of didSelectTab signal since it may be from unintentional tab creation.
                if (type == TabSelectionType.FROM_USER) {
                    if (tab.getId() == mAddedTabId || tab.getId() == lastId) {
                        mAddedTabId = Tab.INVALID_TAB_ID;
                    } else {
                        maybeActivateConditionalTabStrip(ReasonToShow.TAB_SWITCHED);
                    }
                }
                if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)
                        && getTabsToShowForId(lastId).contains(tab)) {
                    return;
                }

                if (TabUiFeatureUtilities.isConditionalTabStripEnabled() && mIsTabGroupUiVisible) {
                    return;
                }
                // TODO(995956): Optimization we can do here if we decided always hide the strip if
                // related tab size down to 1.
                resetTabStripWithRelatedTabsForId(tab.getId());
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                if (!mIsTabGroupUiVisible) return;
                // The strip should hide when users close the second-to-last tab in strip. The
                // tabCountToHide for group is 1 because tab group status is updated with this
                // closure before this method is called.
                int tabCountToHide =
                        TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext) ? 1 : 2;
                List<Tab> tabList = getTabsToShowForId(tab.getId());
                if (tabList.size() == tabCountToHide) {
                    resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
                }
            }

            @Override
            public void didAddTab(Tab tab, int type, @TabCreationState int creationState) {
                if (type == TabLaunchType.FROM_CHROME_UI
                        || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
                    maybeActivateConditionalTabStrip(type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                                    ? ReasonToShow.LONG_PRESS
                                    : ReasonToShow.NEW_TAB);
                }

                if (type == TabLaunchType.FROM_CHROME_UI || type == TabLaunchType.FROM_RESTORE
                        || type == TabLaunchType.FROM_STARTUP) {
                    return;
                }

                if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                        && !TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.getValue()) {
                    return;
                }

                if (type == TabLaunchType.FROM_TAB_GROUP_UI && mIsTabGroupUiVisible) {
                    mModel.set(TabGroupUiProperties.INITIAL_SCROLL_INDEX,
                            getTabsToShowForId(tab.getId()).size() - 1);
                }

                if (mIsTabGroupUiVisible) return;

                resetTabStripWithRelatedTabsForId(tab.getId());
            }

            @Override
            public void restoreCompleted() {
                Tab currentTab = mTabModelSelector.getCurrentTab();
                // Do not try to show tab strip when there is no current tab or we are not in tab
                // page when restore completed.
                if (currentTab == null
                        || (mLayoutStateProvider != null
                                && mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER))) {
                    return;
                }
                resetTabStripWithRelatedTabsForId(currentTab.getId());
                RecordUserAction.record("TabStrip.SessionVisibility."
                        + (mIsTabGroupUiVisible ? "Visible" : "Hidden"));
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                if (!mIsTabGroupUiVisible) {
                    // Reset with the current tab as the undone tab may be in the background.
                    resetTabStripWithRelatedTabsForId(mTabModelSelector.getCurrentTab().getId());
                }
            }
        };
        mLayoutStateObserver =
                new FilterLayoutStateObserver(LayoutType.TAB_SWITCHER, new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType, boolean showToolbar) {
                        assert layoutType == LayoutType.TAB_SWITCHER;

                        maybeActivateConditionalTabStrip(ReasonToShow.TAB_SWITCHED);
                        mIsShowingOverViewMode = true;
                        resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        assert layoutType == LayoutType.TAB_SWITCHER;

                        mIsShowingOverViewMode = false;
                        Tab tab = mTabModelSelector.getCurrentTab();
                        if (tab == null) return;
                        resetTabStripWithRelatedTabsForId(tab.getId());
                    }
                });

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                // TODO(crbug.com/1087826) This is a band-aid fix for M84. The root cause is
                // probably a leaked observer. Remove this when the TabObservers are removed during
                // tab reparenting.
                if (mTabModelSelector.getTabById(tab.getId()) == null) return;
                List<Tab> listOfTabs = getTabsToShowForId(tab.getId());
                int numTabs = listOfTabs.size();
                // This is set to zero because the UI is hidden.
                if (!mIsTabGroupUiVisible || numTabs == 1) numTabs = 0;
                RecordHistogram.recordCount1MHistogram("TabStrip.TabCountOnPageLoad", numTabs);
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
                // Remove this when tab is detached since the TabModelSelectorTabObserver is not
                // properly destroyed when there is a normal/night mode switch.
                if (window == null) {
                    this.destroy();
                    mTabModelSelectorTabObserver = null;
                }
            }
        };

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mSnackbarManager.dismissSnackbars(TabGroupUiMediator.this);
                resetTabStripWithRelatedTabsForId(mTabModelSelector.getCurrentTabId());
            }
        };

        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)) {
            mTabGroupModelFilterObserver = new EmptyTabGroupModelFilterObserver() {
                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    if (mIsTabGroupUiVisible && movedTab == mTabModelSelector.getCurrentTab()) {
                        resetTabStripWithRelatedTabsForId(movedTab.getId());
                    }
                }
            };

            // TODO(995951): Add observer similar to TabModelSelectorTabModelObserver for
            // TabModelFilter.
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     false))
                    .addTabGroupObserver(mTabGroupModelFilterObserver);
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     true))
                    .addTabGroupObserver(mTabGroupModelFilterObserver);
        }

        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            mPauseResumeWithNativeObserver = new PauseResumeWithNativeObserver() {
                @Override
                public void onResumeWithNative() {
                    resetTabStripWithRelatedTabsForId(mTabModelSelector.getCurrentTabId());
                    if (!mTabModelSelector.isTabStateInitialized()) {
                        return;
                    }
                    RecordUserAction.record("TabStrip.SessionVisibility."
                            + (mIsTabGroupUiVisible ? "Visible" : "Hidden"));
                }

                @Override
                public void onPauseWithNative() {
                    resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
                }
            };
            mActivityLifecycleDispatcher.register(mPauseResumeWithNativeObserver);
        }

        if (TabUiFeatureUtilities.isLaunchBugFixEnabled()) {
            mOmniboxFocusObserver = isFocus -> {
                // Hide tab strip when omnibox gains focus and try to re-show it when omnibox loses
                // focus.
                int tabId = (isFocus == null || !isFocus) ? mTabModelSelector.getCurrentTabId()
                                                          : Tab.INVALID_TAB_ID;
                resetTabStripWithRelatedTabsForId(tabId);
            };
            mOmniboxFocusStateSupplier.addObserver(mOmniboxFocusObserver);
        }

        mIncognitoStateObserver = (isIncognito) -> {
            mModel.set(TabGroupUiProperties.IS_INCOGNITO, isIncognito);
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        layoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable((layoutStateProvider) -> {
                    mLayoutStateProvider = layoutStateProvider;
                    mLayoutStateProvider.addObserver(mLayoutStateObserver);
                }));

        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(mIncognitoStateObserver);

        setupToolbarButtons();
        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, true);
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null) {
            resetTabStripWithRelatedTabsForId(tab.getId());
        }
    }

    void setupLeftButtonDrawable(int drawableId) {
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, drawableId);
    }

    void setupLeftButtonOnClickListener(View.OnClickListener listener) {
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, listener);
    }

    private void setupToolbarButtons() {
        View.OnClickListener leftButtonOnClickListener;
        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)) {
            // For tab group, the left button is to show the tab grid dialog.
            leftButtonOnClickListener = view -> {
                Tab currentTab = mTabModelSelector.getCurrentTab();
                if (currentTab == null) return;
                mResetHandler.resetGridWithListOfTabs(getTabsToShowForId(currentTab.getId()));
                RecordUserAction.record("TabGroup.ExpandedFromStrip.TabGridDialog");
            };
        } else {
            // For conditional tab strip, the left button is to dismiss the strip.
            leftButtonOnClickListener = view -> {
                resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
                ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.FORBIDDEN);
                RecordUserAction.record("TabStrip.UserDismissed");
                if (ConditionalTabStripUtils.shouldShowSnackbarForDismissal()) {
                    mSnackbarManager.showSnackbar(mUndoClosureSnackBar);
                }
            };
            mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, R.drawable.btn_close);
        }
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, leftButtonOnClickListener);

        View.OnClickListener rightButtonOnClickListener = view -> {
            Tab parentTabToAttach = null;
            Tab currentTab = mTabModelSelector.getCurrentTab();
            if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)) {
                List<Tab> relatedTabs = getTabsToShowForId(currentTab.getId());

                assert relatedTabs.size() > 0;

                parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
            }
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                            TabLaunchType.FROM_TAB_GROUP_UI, parentTabToAttach);
            RecordUserAction.record("MobileNewTabOpened." + TabGroupUiCoordinator.COMPONENT_NAME);
            if (!currentTab.isIncognito()) {
                ReturnToChromeUtil.onNewTabOpened();
            }
        };
        mModel.set(TabGroupUiProperties.RIGHT_BUTTON_ON_CLICK_LISTENER, rightButtonOnClickListener);

        String leftButtonContentDescription;
        String rightButtonContentDescription;
        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            leftButtonContentDescription =
                    mContext.getString(R.string.accessibility_bottom_tab_strip_close_strip);
            rightButtonContentDescription =
                    mContext.getString(R.string.accessibility_toolbar_btn_new_tab);
        } else {
            leftButtonContentDescription =
                    mContext.getString(R.string.accessibility_bottom_tab_strip_expand_tab_sheet);
            rightButtonContentDescription = mContext.getString(R.string.bottom_tab_grid_new_tab);
        }
        mModel.set(
                TabGroupUiProperties.LEFT_BUTTON_CONTENT_DESCRIPTION, leftButtonContentDescription);
        mModel.set(TabGroupUiProperties.RIGHT_BUTTON_CONTENT_DESCRIPTION,
                rightButtonContentDescription);
    }

    /**
     * Update the tab strip based on given tab ID.
     * @param id  If the ID is set to Tab.INVALID_TAB_ID, this method will hide the tab strip. If
     *            not, associated tabs from #getTabsToShowForID will be showing in the tab strip.
     */
    private void resetTabStripWithRelatedTabsForId(int id) {
        // TODO(crbug.com/1090655): We should be able to guard this call behind some checks so that
        // we can assert here that 1) mIsShowingOverViewMode is false 2) mIsTabGroupUiVisible with
        // valid id is false.
        // When overview mode is showing or conditional tab strip feature is
        // turned on but the feature is not activated (i.e. forbidden or default), keep the tab
        // strip hidden.
        if (mIsShowingOverViewMode
                || (TabUiFeatureUtilities.isConditionalTabStripEnabled()
                        && ConditionalTabStripUtils.getFeatureStatus()
                                != FeatureStatus.ACTIVATED)) {
            id = Tab.INVALID_TAB_ID;
        }
        List<Tab> listOfTabs = getTabsToShowForId(id);
        if (listOfTabs.size() < 2) {
            mResetHandler.resetStripWithListOfTabs(null);
            mIsTabGroupUiVisible = false;
        } else {
            mResetHandler.resetStripWithListOfTabs(listOfTabs);
            mIsTabGroupUiVisible = true;
            if (mActivatedButNotShown) {
                mActivatedButNotShown = false;
                RecordUserAction.record("TabStrip.ShownOnTabUse");
                ConditionalTabStripUtils.updateLastShownTimeStamp();
            }
        }
        if (mIsTabGroupUiVisible) {
            // Post to make sure that the recyclerView already knows how many visible items it has.
            // This is to make sure that we can scroll to a state where the selected tab is in the
            // middle of the strip.
            Handler handler = new Handler();
            handler.post(()
                                 -> mModel.set(TabGroupUiProperties.INITIAL_SCROLL_INDEX,
                                         listOfTabs.indexOf(mTabModelSelector.getCurrentTab())));
        }
        mVisibilityController.setBottomControlsVisible(mIsTabGroupUiVisible);
    }

    /**
     * Get a list of tabs to show based on a tab ID. When tab group is enabled, it will return all
     * tabs that are in the same group with target tab. When conditional tab strip is enabled, it
     * will return all tabs in the same tab model as target tab.
     * @param id  The ID of the tab that will be used to decide the list of tabs to show.
     */
    private List<Tab> getTabsToShowForId(int id) {
        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            ArrayList<Tab> tabList = new ArrayList<>();
            if (id == Tab.INVALID_TAB_ID) return tabList;
            Tab tab = mTabModelSelector.getTabById(id);
            TabModel tabModel = mTabModelSelector.getModel(tab.isIncognito());
            for (int i = 0; i < tabModel.getCount(); i++) {
                tabList.add(tabModel.getTabAt(i));
            }
            return tabList;
        }
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(id);
    }

    public boolean onBackPressed() {
        // TODO(crbug.com/1006421): add a regression test to make sure that the back button closes
        // the dialog when the dialog is showing.
        return mTabGridDialogController != null && mTabGridDialogController.handleBackPressed();
    }

    @Override
    public void handleBackPress() {
        if (mTabGridDialogController != null) mTabGridDialogController.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        if (mTabGridDialogController == null) {
            return BackPressHandler.super.getHandleBackPressChangedSupplier();
        }
        return mTabGridDialogController.getHandleBackPressChangedSupplier();
    }

    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            if (mTabGroupModelFilterObserver != null) {
                ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getTabModelFilter(false))
                        .removeTabGroupObserver(mTabGroupModelFilterObserver);
                ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getTabModelFilter(true))
                        .removeTabGroupObserver(mTabGroupModelFilterObserver);
            }
        }
        if (mPauseResumeWithNativeObserver != null) {
            mActivityLifecycleDispatcher.unregister(mPauseResumeWithNativeObserver);
        }
        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
        }
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mOmniboxFocusObserver != null) {
            mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        }
        mIncognitoStateProvider.removeObserver(mIncognitoStateObserver);
    }

    private void maybeActivateConditionalTabStrip(@ReasonToShow int reason) {
        if (ConditionalTabStripUtils.getFeatureStatus() == FeatureStatus.DEFAULT
                && TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.ACTIVATED);
            mActivatedButNotShown = true;
            RecordHistogram.recordEnumeratedHistogram("TabStrip.ReasonToShow", reason,
                    ConditionalTabStripUtils.ReasonToShow.NUM_ENTRIES);
        }
    }

    // SnackbarManager.SnackbarController implementation.
    @Override
    public void onAction(Object actionData) {
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.ACTIVATED);
        resetTabStripWithRelatedTabsForId(mTabModelSelector.getCurrentTabId());
        RecordUserAction.record("TabStrip.UndoDismiss");
    }

    @VisibleForTesting
    boolean getIsShowingOverViewModeForTesting() {
        return mIsShowingOverViewMode;
    }

    @VisibleForTesting
    int getConditionalTabStripFeatureStatusForTesting() {
        return ConditionalTabStripUtils.getFeatureStatus();
    }
}
