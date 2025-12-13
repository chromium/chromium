// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.UNINITIALIZED;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREBackPressType;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFRERestoredTabsResult;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREResultAction;
import org.chromium.ui.modelutil.PropertyModel;

/** Handles back press on the current screen of the Restore Tabs promo. */
@NullMarked
class RestoreTabsBackPressHandler {
    private final PropertyModel mModel;

    RestoreTabsBackPressHandler(PropertyModel model) {
        mModel = model;
    }

    void backPressOnCurrentScreen() {
        @RestoreTabsProperties.ScreenType
        int currentScreen = mModel.get(RestoreTabsProperties.CURRENT_SCREEN);

        switch (currentScreen) {
            case DEVICE_SCREEN:
                mModel.set(RestoreTabsProperties.CURRENT_SCREEN, HOME_SCREEN);
                break;
            case REVIEW_TABS_SCREEN:
                mModel.set(RestoreTabsProperties.CURRENT_SCREEN, HOME_SCREEN);
                break;
            case HOME_SCREEN:
                mModel.set(RestoreTabsProperties.VISIBLE, false);
                RestoreTabsMetricsHelper.recordResultActionHistogram(
                        RestoreTabsOnFREResultAction.DISMISSED_BACKPRESS);
                RestoreTabsMetricsHelper.recordResultActionMetrics(
                        RestoreTabsOnFREResultAction.DISMISSED_BACKPRESS);
                RestoreTabsMetricsHelper.recordRestoredTabsResultHistogram(
                        RestoreTabsOnFRERestoredTabsResult.NONE);
                break;
            default:
                assert currentScreen == UNINITIALIZED : "Back pressing on an unidentified screen.";
        }

        if (currentScreen != UNINITIALIZED) {
            RestoreTabsMetricsHelper.recordBackPressTypeMetrics(
                    RestoreTabsOnFREBackPressType.SYSTEM_BACKPRESS);
        }
    }
}
