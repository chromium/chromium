// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic to set the state of the model and react to events like clicks.
 */
public class RestoreTabsMediator {
    private RestoreTabsControllerFactory.ControllerListener mListener;
    private PropertyModel mModel;

    public void initialize(
            PropertyModel model, RestoreTabsControllerFactory.ControllerListener listener) {
        mListener = listener;
        mModel = model;
        mModel.set(RestoreTabsProperties.HOME_SCREEN_DELEGATE, createHomeScreenDelegate());
        mModel.set(RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER,
                () -> { setCurrentScreen(RestoreTabsProperties.ScreenType.HOME_SCREEN); });
    }

    public void destroy() {
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
                mModel.set(RestoreTabsProperties.VISIBLE, false);
            };

            @Override
            public void onReviewTabsChosen() {
                setCurrentScreen(RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN);
            }
        };
    }

    public void showOptions(List<ForeignSession> sessions) {
        setDeviceListItems(sessions);
        setCurrentScreen(mModel.get(RestoreTabsProperties.CURRENT_SCREEN));
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
    public void setDeviceListItems(List<ForeignSession> sessions) {
        assert sessions != null && sessions.size() != 0;

        ForeignSession previousSelection = mModel.get(RestoreTabsProperties.SELECTED_DEVICE);
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
    public void setSelectedDeviceItem(ForeignSession selectedSession) {
        assert selectedSession != null;
        mModel.set(RestoreTabsProperties.SELECTED_DEVICE, selectedSession);

        ModelList allItems = mModel.get(RestoreTabsProperties.DEVICE_MODEL_LIST);
        for (ListItem item : allItems) {
            boolean isSelected = selectedSession.equals(
                    item.model.get(ForeignSessionItemProperties.SESSION_PROFILE));
            item.model.set(ForeignSessionItemProperties.IS_SELECTED, isSelected);
        }
    }

    /**
     * Selects the currently shown screen on the bottomsheet.
     * @param screenType A {@link RestoreTabsProperties.ScreenType} that defines the screen to be
     *         shown.
     */
    public void setCurrentScreen(int screenType) {
        if (screenType == RestoreTabsProperties.ScreenType.DEVICE_SCREEN) {
            mModel.set(RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST,
                    mModel.get(RestoreTabsProperties.DEVICE_MODEL_LIST));
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
            public void onChangeSelectionStateForAllTabs() {}

            @Override
            public void onSelectedTabsChosen() {
                mModel.set(RestoreTabsProperties.VISIBLE, false);
            };
        };
    }
}
