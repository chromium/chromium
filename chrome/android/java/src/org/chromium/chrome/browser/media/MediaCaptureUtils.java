// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

/** Shared utility methods for media capture picker and sharing sessions. */
@NullMarked
public class MediaCaptureUtils {
    private static @Nullable Callback<Tab> sBringTabToFrontCallbackForTesting;

    /**
     * Move the window of the given tab to the front, with the tab selected if it is from a Chrome
     * tabbed activity, ensuring the tab is visible.
     *
     * @param context The Android context.
     * @param tab The tab to be brought forward.
     */
    public static void bringTabToFront(Context context, Tab tab) {
        if (sBringTabToFrontCallbackForTesting != null) {
            sBringTabToFrontCallbackForTesting.onResult(tab);
            return;
        }

        Activity activity = tab.getWindowAndroidChecked().getActivity().get();
        if (activity == null) {
            return;
        }

        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(activity);
        int taskId = activity.getTaskId();
        AppTask appTask = AndroidTaskUtils.getAppTaskFromId(activity, taskId);

        boolean success =
                (windowId != TabWindowManager.INVALID_WINDOW_ID)
                        && MultiWindowUtils.launchIntentInInstance(
                                IntentHandler.createTrustedBringTabToFrontIntent(
                                        tab.getId(), IntentHandler.BringToFrontSource.ACTIVATE_TAB),
                                windowId);

        if (!success && appTask != null) {
            try {
                appTask.startActivity(context, new Intent(activity, activity.getClass()), null);
                success = true;
            } catch (Exception ignored) {
            }
        }

        if (!success) {
            ApiCompatibilityUtils.moveTaskToFront(activity, taskId, 0);
        }
    }

    public static void setBringTabToFrontCallbackForTesting(@Nullable Callback<Tab> callback) {
        sBringTabToFrontCallbackForTesting = callback;
        ResettersForTesting.register(() -> sBringTabToFrontCallbackForTesting = null);
    }
}
