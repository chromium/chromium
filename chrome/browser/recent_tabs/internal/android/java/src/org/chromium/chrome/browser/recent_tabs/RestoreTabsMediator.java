// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic to set the state of the model and react to events like clicks.
 */
public class RestoreTabsMediator {
    private PropertyModel mModel;

    public void initialize(PropertyModel model) {
        mModel = model;
        mModel.set(RestoreTabsProperties.HOME_SCREEN_DELEGATE, createHomeScreenDelegate());
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

    public void setDeviceListItems(List<ForeignSession> sessions) {
        assert sessions != null && sessions.size() != 0;

        ForeignSession previousSelection = mModel.get(RestoreTabsProperties.SELECTED_DEVICE);
        ForeignSession newSelection = sessions.get(0);

        setSelectedDeviceItem(newSelection);
    }

    public void setSelectedDeviceItem(ForeignSession selectedSession) {
        assert selectedSession != null;
        mModel.set(RestoreTabsProperties.SELECTED_DEVICE, selectedSession);
    }

    /**
     * Selects the currently shown screen on the bottomsheet.
     * @param screenType A {@link RestoreTabsProperties.ScreenType} that defines the screen to be
     *         shown.
     */
    public void setCurrentScreen(int screenType) {}
}
