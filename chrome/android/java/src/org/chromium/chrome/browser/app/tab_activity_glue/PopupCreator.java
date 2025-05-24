// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Intent;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Pair;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/** Handles launching new popup windows as CCTs. */
public class PopupCreator {
    private static Boolean sArePopupsEnabledForTesting;
    private static ReparentingTask sReparentingTaskForTesting;

    // TODO(https://crbug.com/411002260): remove the display argument when Android display topology
    // API is available in Chrome
    public static void moveTabToNewPopup(
            Tab tab, WindowFeatures windowFeatures, DisplayAndroid display) {
        Intent intent = initializePopupIntent();
        ActivityOptions activityOptions = createPopupActivityOptions(windowFeatures, display);

        getReparentingTask(tab)
                .begin(
                        ContextUtils.getApplicationContext(),
                        intent,
                        activityOptions.toBundle(),
                        null);
    }

    public static boolean arePopupsEnabled(Activity activity) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)) {
            return false;
        }

        if (sArePopupsEnabledForTesting != null) {
            return sArePopupsEnabledForTesting;
        }

        // TODO(https://crbug.com/411013760): update this when relevant Android API is landed
        return activity.isInMultiWindowMode();
    }

    public static void setArePopupsEnabledForTesting(boolean value) {
        sArePopupsEnabledForTesting = value;
        ResettersForTesting.register(() -> sArePopupsEnabledForTesting = null);
    }

    public static void setReparentingTaskForTesting(ReparentingTask task) {
        sReparentingTaskForTesting = task;
        ResettersForTesting.register(() -> sReparentingTaskForTesting = null);
    }

    private static Intent initializePopupIntent() {
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), CustomTabActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);

        return intent;
    }

    private static ActivityOptions createPopupActivityOptions(
            WindowFeatures windowFeatures, DisplayAndroid display) {
        ActivityOptions activityOptions = ActivityOptions.makeBasic();

        Pair<Integer, Rect> localCoordinatesPx =
                getLocalCoordinatesPxFromWindowFeatures(windowFeatures, display);
        if (localCoordinatesPx.first != null) {
            activityOptions.setLaunchDisplayId(localCoordinatesPx.first);
            if (localCoordinatesPx.second != null) {
                activityOptions.setLaunchBounds(localCoordinatesPx.second);
            }
        }

        return activityOptions;
    }

    private static Pair<Integer, Rect> getLocalCoordinatesPxFromWindowFeatures(
            WindowFeatures windowFeatures, DisplayAndroid display) {
        if (windowFeatures.width == null || windowFeatures.height == null) {
            return Pair.create(null, null);
        }

        float widthDp = windowFeatures.width;
        float heightDp = windowFeatures.height;
        float leftDp = windowFeatures.left == null ? 0 : windowFeatures.left;
        float topDp = windowFeatures.top == null ? 0 : windowFeatures.top;

        float rightDp = leftDp + widthDp;
        float bottomDp = topDp + heightDp;

        return DisplayUtil.getLocalCoordinatesPx(
                new RectF(leftDp, topDp, rightDp, bottomDp), display);
    }

    private static ReparentingTask getReparentingTask(Tab tab) {
        if (sReparentingTaskForTesting != null) {
            return sReparentingTaskForTesting;
        }

        return ReparentingTask.from(tab);
    }
}
