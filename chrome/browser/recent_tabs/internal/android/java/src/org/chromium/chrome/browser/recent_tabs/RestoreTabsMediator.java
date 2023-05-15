// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Contains the logic to set the state of the model and react to events like clicks.
 */
public class RestoreTabsMediator {
    private RestoreTabsControllerFactory.ControllerListener mListener;
    private PropertyModel mModel;
    private ForeignSessionHelper mForeignSessionHelper;
    private TabCreatorManager mTabCreatorManager;

    public void initialize(PropertyModel model,
            RestoreTabsControllerFactory.ControllerListener listener, Profile profile,
            TabCreatorManager tabCreatorManager) {
        initialize(model, listener, new ForeignSessionHelper(profile), tabCreatorManager);
    }

    protected void initialize(PropertyModel model,
            RestoreTabsControllerFactory.ControllerListener listener,
            ForeignSessionHelper foreignSessionHelper, TabCreatorManager tabCreatorManager) {
        mListener = listener;
        mForeignSessionHelper = foreignSessionHelper;
        mTabCreatorManager = tabCreatorManager;
        mModel = model;
        mModel.set(RestoreTabsProperties.HOME_SCREEN_DELEGATE, createHomeScreenDelegate());
        mModel.set(RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER,
                () -> { setCurrentScreen(RestoreTabsProperties.ScreenType.HOME_SCREEN); });
    }

    public void destroy() {
        mForeignSessionHelper.destroy();
        mForeignSessionHelper = null;
        mModel.set(RestoreTabsProperties.VISIBLE, false);
    }

    /** Returns an implementation the RestoreTabsPromoScreen.Delegate interface. */
    private RestoreTabsPromoScreenCoordinator.Delegate createHomeScreenDelegate() {
        return new RestoreTabsPromoScreenCoordinator.Delegate() {
            @Override
            public void onShowDeviceList() {
                setCurrentScreen(RestoreTabsProperties.ScreenType.DEVICE_SCREEN);
            }

            @Override
            public void onAllTabsChosen() {
                restoreChosenTabs();
            };

            @Override
            public void onReviewTabsChosen() {
                setCurrentScreen(RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN);
            }
        };
    }

    public void showHomeScreen() {
        if (mModel.get(RestoreTabsProperties.CURRENT_SCREEN)
                == RestoreTabsProperties.ScreenType.HOME_SCREEN) {
            return;
        }

        setDeviceListItems(mForeignSessionHelper.getMobileAndTabletForeignSessions());
        setTabListItems();

        // On initialization, the current screen is not set to prevent re-setting the home screen at
        // this call site. Some property keys like HOME_SCREEN_DELEGATE are set after initialization
        // and with the streamlined binding of keys based on screen type, logic for those keys will
        // not be run until the home screen is set here, re-binding all the screen relevant keys.
        setCurrentScreen(RestoreTabsProperties.ScreenType.HOME_SCREEN);
        mModel.set(RestoreTabsProperties.VISIBLE, true);
    }

    /** Dismiss the bottom sheet */
    public void dismiss() {
        mModel.set(RestoreTabsProperties.VISIBLE, false);

        if (mListener != null) {
            mListener.onDismissed();
        }
    }

    /**
     * Sets the device items and creates the corresponding models for the
     * device item entries on the device profiles page.
     * If there is a selected device profile prior to calling this method, the device
     * with the same tag will remain selected. If no prior selection was made or this
     * tag no longer exists, the first device is selected.
     * @param sessions The list of ForeignSession to set as device profiles.
     */
    @VisibleForTesting
    public void setDeviceListItems(List<ForeignSession> sessions) {
        assert sessions != null && sessions.size() != 0;

        ForeignSession previousSelection = mModel.get(RestoreTabsProperties.SELECTED_DEVICE);

        // Sort the incoming list of foreign sessions by the most recent modified time.
        Collections.sort(sessions,
                (ForeignSession s1,
                        ForeignSession s2) -> Long.compare(s2.modifiedTime, s1.modifiedTime));
        ForeignSession newSelection = sessions.get(0);

        // Populate all model entries.
        ModelList sessionItems = mModel.get(RestoreTabsProperties.DEVICE_MODEL_LIST);
        sessionItems.clear();
        for (ForeignSession session : sessions) {
            if (previousSelection != null && session.tag.equals(previousSelection.tag)) {
                newSelection = session;
            }
            PropertyModel model = ForeignSessionItemProperties.create(
                    /*session=*/session, /*isSelected=*/false, /*onClickListener=*/() -> {
                        setSelectedDeviceItem(session);
                        setCurrentScreen(RestoreTabsProperties.ScreenType.HOME_SCREEN);
                    });
            sessionItems.add(new ListItem(DetailItemType.DEVICE, model));
        }

        setSelectedDeviceItem(newSelection);
    }

    /**
     * Sets the selected device profile and updates the IS_SELECTED entry in the models
     * of the device item entries on the device profiles page.
     * @param selectedSession The device that is to be selected.
     */
    @VisibleForTesting
    public void setSelectedDeviceItem(ForeignSession selectedSession) {
        assert selectedSession != null;
        mModel.set(RestoreTabsProperties.SELECTED_DEVICE, selectedSession);

        ModelList allItems = mModel.get(RestoreTabsProperties.DEVICE_MODEL_LIST);
        for (ListItem item : allItems) {
            boolean isSelected = selectedSession.equals(
                    item.model.get(ForeignSessionItemProperties.SESSION_PROFILE));
            item.model.set(ForeignSessionItemProperties.IS_SELECTED, isSelected);
        }

        // After selecting a device, rebuild all the tab list items for the new selection.
        setTabListItems();
    }

    /**
     * Sets the tab items and creates the corresponding models for the
     * tab item entries on the tab list page. All tabs will be selected by default.
     */
    @VisibleForTesting
    public void setTabListItems() {
        // TODO(crbug.com/1429406): Refactor ForeignSessionHelper to retrieve only the
        // necessary data instead of preloading all the sessions.
        ForeignSession session = mModel.get(RestoreTabsProperties.SELECTED_DEVICE);
        assert session != null;

        List<ForeignSessionWindow> windows = session.windows;
        List<ForeignSessionTab> tabs = new ArrayList<>();

        // Flatten all tabs in every window into one list
        for (ForeignSessionWindow window : windows) {
            tabs.addAll(window.tabs);
        }

        // Populate all model entries.
        ModelList tabItems = mModel.get(RestoreTabsProperties.REVIEW_TABS_MODEL_LIST);
        tabItems.clear();
        for (ForeignSessionTab tab : tabs) {
            PropertyModel model = TabItemProperties.create(/*tab=*/tab, /*isSelected=*/true);
            model.set(
                    TabItemProperties.ON_CLICK_LISTENER, () -> { toggleTabSelectedState(model); });
            tabItems.add(new ListItem(DetailItemType.TAB, model));
        }
    }

    /**
     * Toggles the selected tab and updates the IS_SELECTED entry in the models of the tab entries
     * on the tab list page. Also updates the NUM_TABS_DESELECTED entry in the general model to
     * track the deselect/select option
     * @param model The property model from TabItemProperties that is associated with the tab that
     * is having its selection toggled.
     */
    @VisibleForTesting
    public void toggleTabSelectedState(PropertyModel model) {
        assert model.get(TabItemProperties.FOREIGN_SESSION_TAB) != null;
        boolean wasSelected = model.get(TabItemProperties.IS_SELECTED);
        model.set(TabItemProperties.IS_SELECTED, !wasSelected);

        // If the tab was selected then it will get deselected, and vice versa.
        int numTabsDeselected = mModel.get(RestoreTabsProperties.NUM_TABS_DESELECTED);
        if (wasSelected) {
            numTabsDeselected++;
        } else {
            numTabsDeselected--;
        }

        mModel.set(RestoreTabsProperties.NUM_TABS_DESELECTED, numTabsDeselected);
    }

    /**
     * Selects the currently shown screen on the bottomsheet.
     * @param screenType A {@link RestoreTabsProperties.ScreenType} that defines the screen to be
     *         shown.
     */
    @VisibleForTesting
    public void setCurrentScreen(int screenType) {
        if (screenType == RestoreTabsProperties.ScreenType.DEVICE_SCREEN) {
            mModel.set(RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST,
                    mModel.get(RestoreTabsProperties.DEVICE_MODEL_LIST));
            mModel.set(RestoreTabsProperties.DETAIL_SCREEN_TITLE,
                    R.string.restore_tabs_device_screen_sheet_title);
            mModel.set(RestoreTabsProperties.REVIEW_TABS_SCREEN_DELEGATE, null);
        } else if (screenType == RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN) {
            mModel.set(RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST,
                    mModel.get(RestoreTabsProperties.REVIEW_TABS_MODEL_LIST));
            mModel.set(RestoreTabsProperties.REVIEW_TABS_SCREEN_DELEGATE,
                    createReviewTabsScreenDelegate());
        } else {
            mModel.set(RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST, null);
        }

        mModel.set(RestoreTabsProperties.CURRENT_SCREEN, screenType);
    }

    /** Returns an implementation of the RestoreTabsDetailScreen.Delegate interface. */
    private RestoreTabsDetailScreenCoordinator.Delegate createReviewTabsScreenDelegate() {
        return new RestoreTabsDetailScreenCoordinator.Delegate() {
            // If all tabs are selected this will present a deselect all option, otherwise it will
            // present a select all option.
            @Override
            public void onChangeSelectionStateForAllTabs() {
                ModelList allItems = mModel.get(RestoreTabsProperties.REVIEW_TABS_MODEL_LIST);
                boolean allTabsSelected =
                        mModel.get(RestoreTabsProperties.NUM_TABS_DESELECTED) == 0;
                for (ListItem item : allItems) {
                    item.model.set(TabItemProperties.IS_SELECTED, !allTabsSelected);
                }

                // If all tabs are currently selected, then they will be deselected and vice versa.
                if (allTabsSelected) {
                    mModel.set(RestoreTabsProperties.NUM_TABS_DESELECTED, allItems.size());
                } else {
                    mModel.set(RestoreTabsProperties.NUM_TABS_DESELECTED, 0);
                }
            }

            @Override
            public void onSelectedTabsChosen() {
                restoreChosenTabs();
            };
        };
    }

    private void restoreChosenTabs() {
        if (!mModel.get(RestoreTabsProperties.VISIBLE)) {
            return; // Dismiss only if not dismissed yet.
        }

        List<ForeignSessionTab> tabs = new ArrayList<>();
        ModelList selectedTabs = mModel.get(RestoreTabsProperties.REVIEW_TABS_MODEL_LIST);
        for (ListItem item : selectedTabs) {
            if (item.model.get(TabItemProperties.IS_SELECTED)) {
                tabs.add(item.model.get(TabItemProperties.FOREIGN_SESSION_TAB));
            }
        }

        // TODO(crbug.com/1426921): Consider adding a safeguard for not allowing restoration
        // below 1, and adding a spinner if restoring the tabs becomes a batched process.
        assert selectedTabs.size() > 0;
        mForeignSessionHelper.openForeignSessionTabsAsBackgroundTabs(
                tabs, mModel.get(RestoreTabsProperties.SELECTED_DEVICE), mTabCreatorManager);
        dismiss();
    }
}
