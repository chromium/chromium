// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.os.Handler;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
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
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/** A mediator for the TabGroupUi. Responsible for managing the internal state of the component. */
public class TabGroupUiMediator implements BackPressHandler {
    /** An interface to control the TabGroupUi component. */
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
    private final BottomControlsCoordinator.BottomControlsVisibilityController
            mVisibilityController;
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final LazyOneshotSupplier<DialogController> mTabGridDialogControllerSupplier;
    private final IncognitoStateObserver mIncognitoStateObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final ObservableSupplierImpl<Boolean> mHandleBackPressChangedSupplier;

    private CallbackController mCallbackController = new CallbackController();
    private final LayoutStateObserver mLayoutStateObserver;
    private LayoutStateProvider mLayoutStateProvider;

    private TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private Callback<Boolean> mOmniboxFocusObserver;
    private boolean mIsTabGroupUiVisible;
    private boolean mIsShowingOverViewMode;
    private final @ColorInt int mPrimaryBackgroundColor;
    private final @ColorInt int mIncognitoBackgroundColor;

    TabGroupUiMediator(
            Context context,
            BottomControlsVisibilityController visibilityController,
            ObservableSupplierImpl<Boolean> handleBackPressChangedSupplier,
            ResetHandler resetHandler,
            PropertyModel model,
            TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            IncognitoStateProvider incognitoStateProvider,
            @Nullable
                    LazyOneshotSupplier<TabGridDialogMediator.DialogController>
                            dialogControllerSupplier,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        this(
                context,
                visibilityController,
                handleBackPressChangedSupplier,
                resetHandler,
                model,
                tabModelSelector,
                tabCreatorManager,
                layoutStateProviderSupplier,
                incognitoStateProvider,
                dialogControllerSupplier,
                omniboxFocusStateSupplier,
                SemanticColorUtils.getDialogBgColor(context),
                context.getColor(org.chromium.chrome.R.color.dialog_bg_color_dark_baseline));
    }

    @VisibleForTesting
    TabGroupUiMediator(
            Context context,
            BottomControlsVisibilityController visibilityController,
            ObservableSupplierImpl<Boolean> handleBackPressChangedSupplier,
            ResetHandler resetHandler,
            PropertyModel model,
            TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            IncognitoStateProvider incognitoStateProvider,
            @Nullable
                    LazyOneshotSupplier<TabGridDialogMediator.DialogController>
                            dialogControllerSupplier,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @ColorInt int primaryBackgroundColor,
            @ColorInt int incognitoBackgroundColor) {
        mContext = context;
        mResetHandler = resetHandler;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mVisibilityController = visibilityController;
        mIncognitoStateProvider = incognitoStateProvider;
        mTabGridDialogControllerSupplier = dialogControllerSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mPrimaryBackgroundColor = primaryBackgroundColor;
        mIncognitoBackgroundColor = incognitoBackgroundColor;

        var layoutStateProvider = layoutStateProviderSupplier.get();
        if (layoutStateProvider != null
                && layoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            mIsShowingOverViewMode = true;
        }

        // register for tab model
        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (getTabsToShowForId(lastId).contains(tab)) {
                            return;
                        }

                        resetTabStripWithRelatedTabsForId(tab.getId());
                    }

                    // TODO(crbug/41496693): Delete this logic once tab groups with one tab are
                    // launched.
                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        if (!mIsTabGroupUiVisible) return;

                        // Check if the group the tab was part of is still a tab group.
                        TabGroupModelFilter filter = getCurrentTabGroupModelFilter();
                        Tab groupTab = filter.getGroupLastShownTab(tab.getRootId());
                        if (groupTab == null) return;

                        if (!getCurrentTabGroupModelFilter().isTabInTabGroup(groupTab)) {
                            resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
                        }
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        if (type == TabLaunchType.FROM_CHROME_UI
                                || type == TabLaunchType.FROM_RESTORE
                                || type == TabLaunchType.FROM_STARTUP
                                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
                            return;
                        }

                        if (type == TabLaunchType.FROM_TAB_GROUP_UI && mIsTabGroupUiVisible) {
                            mModel.set(
                                    TabGroupUiProperties.INITIAL_SCROLL_INDEX,
                                    getTabsToShowForId(tab.getId()).size() - 1);
                        }

                        if (mIsTabGroupUiVisible) return;

                        resetTabStripWithRelatedTabsForId(tab.getId());
                    }

                    @Override
                    public void restoreCompleted() {
                        Tab currentTab = mTabModelSelector.getCurrentTab();
                        // Do not try to show tab strip when there is no current tab or we are not
                        // in tab page when restore completed.
                        if (currentTab == null
                                || (mLayoutStateProvider != null
                                        && (mLayoutStateProvider.isLayoutVisible(
                                                LayoutType.TAB_SWITCHER)))) {
                            return;
                        }
                        resetTabStripWithRelatedTabsForId(currentTab.getId());
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        if (!mIsTabGroupUiVisible) {
                            // Reset with the current tab as the undone tab may be in the
                            // background.
                            resetTabStripWithRelatedTabsForId(
                                    mTabModelSelector.getCurrentTab().getId());
                        }
                    }
                };
        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mIsShowingOverViewMode = true;
                            resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
                        }
                    }

                    @Override
                    public void onFinishedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mIsShowingOverViewMode = false;
                            Tab tab = mTabModelSelector.getCurrentTab();
                            if (tab == null) return;
                            resetTabStripWithRelatedTabsForId(tab.getId());
                        }
                    }
                };

        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(mTabModelSelector) {
                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        // TODO(crbug.com/40695094) This is a band-aid fix for M84. The root cause
                        // is
                        // probably a leaked observer. Remove this when the TabObservers are removed
                        // during tab reparenting.
                        if (mTabModelSelector.getTabById(tab.getId()) == null) return;

                        int numTabs = 0;
                        TabGroupModelFilter filter = getCurrentTabGroupModelFilter();
                        if (mIsTabGroupUiVisible && filter.isTabInTabGroup(tab)) {
                            numTabs = filter.getRelatedTabCountForRootId(tab.getRootId());
                        }

                        RecordHistogram.recordCount1MHistogram(
                                "TabStrip.TabCountOnPageLoad", numTabs);
                    }

                    @Override
                    public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
                        // Remove this when tab is detached since the TabModelSelectorTabObserver is
                        // not properly destroyed when there is a normal/night mode switch.
                        if (window == null) {
                            this.destroy();
                            mTabModelSelectorTabObserver = null;
                        }
                    }
                };

        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                        resetTabStripWithRelatedTabsForId(mTabModelSelector.getCurrentTabId());
                    }
                };

        mTabGroupModelFilterObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                        if (mIsTabGroupUiVisible && movedTab == mTabModelSelector.getCurrentTab()) {
                            resetTabStripWithRelatedTabsForId(movedTab.getId());
                        }
                    }
                };

        var filterProvider = mTabModelSelector.getTabModelFilterProvider();
        ((TabGroupModelFilter) filterProvider.getTabModelFilter(false))
                .addTabGroupObserver(mTabGroupModelFilterObserver);
        ((TabGroupModelFilter) filterProvider.getTabModelFilter(true))
                .addTabGroupObserver(mTabGroupModelFilterObserver);

        mOmniboxFocusObserver =
                isFocus -> {
                    // Hide tab strip when omnibox gains focus and try to re-show it when omnibox
                    // loses focus.
                    int tabId =
                            (isFocus == null || !isFocus)
                                    ? mTabModelSelector.getCurrentTabId()
                                    : Tab.INVALID_TAB_ID;
                    resetTabStripWithRelatedTabsForId(tabId);
                };
        mOmniboxFocusStateSupplier.addObserver(mOmniboxFocusObserver);

        mIncognitoStateObserver =
                (isIncognito) -> {
                    mModel.set(TabGroupUiProperties.IS_INCOGNITO, isIncognito);
                    setBottomControlsBackgroundColor(isIncognito);
                };

        filterProvider.addTabModelFilterObserver(mTabModelObserver);
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        if (layoutStateProvider != null) {
            setLayoutStateProvider(layoutStateProvider);
        } else {
            layoutStateProviderSupplier.onAvailable(
                    mCallbackController.makeCancelable(this::setLayoutStateProvider));
        }

        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(mIncognitoStateObserver);

        setupToolbarButtons();
        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, true);
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null) {
            resetTabStripWithRelatedTabsForId(tab.getId());
        }

        mHandleBackPressChangedSupplier = handleBackPressChangedSupplier;
        if (mTabGridDialogControllerSupplier != null) {
            mTabGridDialogControllerSupplier.onAvailable(
                    controller -> {
                        controller
                                .getHandleBackPressChangedSupplier()
                                .addObserver(mHandleBackPressChangedSupplier::set);
                    });
        }
    }

    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        layoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void setBottomControlsBackgroundColor(boolean isIncognito) {
        @ColorInt
        int backgroundColor = isIncognito ? mIncognitoBackgroundColor : mPrimaryBackgroundColor;
        mVisibilityController.setBottomControlsColor(backgroundColor);
        mModel.set(TabGroupUiProperties.BACKGROUND_COLOR, backgroundColor);
    }

    void setupLeftButtonDrawable(int drawableId) {
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, drawableId);
    }

    void setupLeftButtonOnClickListener(View.OnClickListener listener) {
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, listener);
    }

    private void setupToolbarButtons() {
        View.OnClickListener leftButtonOnClickListener =
                view -> {
                    // Don't handle taps until fully visible and done animating.
                    @Nullable DialogController controller = getTabGridDialogControllerIfExists();
                    if (controller != null && controller.getShowingOrAnimationSupplier().get()) {
                        return;
                    }

                    Tab currentTab = mTabModelSelector.getCurrentTab();
                    if (currentTab == null) return;
                    mResetHandler.resetGridWithListOfTabs(getTabsToShowForId(currentTab.getId()));
                    RecordUserAction.record("TabGroup.ExpandedFromStrip.TabGridDialog");
                };
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, leftButtonOnClickListener);

        View.OnClickListener rightButtonOnClickListener =
                view -> {
                    Tab parentTabToAttach = null;
                    Tab currentTab = mTabModelSelector.getCurrentTab();
                    List<Tab> relatedTabs = getTabsToShowForId(currentTab.getId());

                    assert relatedTabs.size() > 0;

                    parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
                    mTabCreatorManager
                            .getTabCreator(currentTab.isIncognito())
                            .createNewTab(
                                    new LoadUrlParams(UrlConstants.NTP_URL),
                                    TabLaunchType.FROM_TAB_GROUP_UI,
                                    parentTabToAttach);
                    RecordUserAction.record(
                            "MobileNewTabOpened." + TabGroupUiCoordinator.COMPONENT_NAME);
                };
        mModel.set(TabGroupUiProperties.RIGHT_BUTTON_ON_CLICK_LISTENER, rightButtonOnClickListener);

        String leftButtonContentDescription =
                mContext.getString(R.string.accessibility_bottom_tab_strip_expand_tab_sheet);
        String rightButtonContentDescription = mContext.getString(R.string.bottom_tab_grid_new_tab);
        mModel.set(
                TabGroupUiProperties.LEFT_BUTTON_CONTENT_DESCRIPTION, leftButtonContentDescription);
        mModel.set(
                TabGroupUiProperties.RIGHT_BUTTON_CONTENT_DESCRIPTION,
                rightButtonContentDescription);
    }

    /**
     * Update the tab strip based on given tab ID.
     *
     * @param id If the ID is set to Tab.INVALID_TAB_ID, this method will hide the tab strip. If
     *     not, associated tabs from #getTabsToShowForID will be showing in the tab strip.
     */
    private void resetTabStripWithRelatedTabsForId(int id) {
        if (!mTabModelSelector.isTabStateInitialized()) return;

        // TODO(crbug.com/40133857): We should be able to guard this call behind some checks so that
        // we can assert here that 1) mIsShowingOverViewMode is false 2) mIsTabGroupUiVisible with
        // valid id is false.
        // When overview mode is showing keep the tab strip hidden.
        if (mIsShowingOverViewMode) {
            id = Tab.INVALID_TAB_ID;
        }
        Tab tab = mTabModelSelector.getTabById(id);
        if (tab == null || !getCurrentTabGroupModelFilter().isTabInTabGroup(tab)) {
            mResetHandler.resetStripWithListOfTabs(null);
            mIsTabGroupUiVisible = false;
        } else {
            List<Tab> listOfTabs = getTabsToShowForId(id);
            mResetHandler.resetStripWithListOfTabs(listOfTabs);
            mIsTabGroupUiVisible = true;

            // Post to make sure that the recyclerView already knows how many visible items it has.
            // This is to make sure that we can scroll to a state where the selected tab is in the
            // middle of the strip.
            Handler handler = new Handler();
            handler.post(
                    () ->
                            mModel.set(
                                    TabGroupUiProperties.INITIAL_SCROLL_INDEX,
                                    listOfTabs.indexOf(mTabModelSelector.getCurrentTab())));
        }
        mVisibilityController.setBottomControlsVisible(mIsTabGroupUiVisible);
    }

    /**
     * Get a list of tabs to show based on a tab ID. When tab group is enabled, it will return all
     * tabs that are in the same group with target tab.
     * @param id  The ID of the tab that will be used to decide the list of tabs to show.
     */
    private List<Tab> getTabsToShowForId(int id) {
        return getCurrentTabGroupModelFilter().getRelatedTabList(id);
    }

    private TabGroupModelFilter getCurrentTabGroupModelFilter() {
        return (TabGroupModelFilter)
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
    }

    public boolean onBackPressed() {
        // TODO(crbug.com/40099884): add a regression test to make sure that the back button closes
        // the dialog when the dialog is showing.
        @Nullable DialogController controller = getTabGridDialogControllerIfExists();
        return controller != null ? controller.handleBackPressed() : false;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        @Nullable DialogController controller = getTabGridDialogControllerIfExists();
        if (controller != null) {
            return controller.handleBackPress();
        }
        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    public void destroy() {
        if (mTabModelSelector != null) {
            var filterProvider = mTabModelSelector.getTabModelFilterProvider();

            filterProvider.removeTabModelFilterObserver(mTabModelObserver);
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            if (mTabGroupModelFilterObserver != null) {
                ((TabGroupModelFilter) filterProvider.getTabModelFilter(false))
                        .removeTabGroupObserver(mTabGroupModelFilterObserver);
                ((TabGroupModelFilter) filterProvider.getTabModelFilter(true))
                        .removeTabGroupObserver(mTabGroupModelFilterObserver);
            }
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

    boolean getIsShowingOverViewModeForTesting() {
        return mIsShowingOverViewMode;
    }

    private @Nullable DialogController getTabGridDialogControllerIfExists() {
        if (mTabGridDialogControllerSupplier == null) return null;
        if (!mTabGridDialogControllerSupplier.hasValue()) return null;
        return mTabGridDialogControllerSupplier.get();
    }
}
