// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.HashSet;
import java.util.Set;

/** Service that handles the action of clicking on the incognito notification. */
public class IncognitoNotificationServiceImpl extends IncognitoNotificationService.Impl {
    private static final String ACTION_CLOSE_ALL_INCOGNITO =
            "com.google.android.apps.chrome.incognito.CLOSE_ALL_INCOGNITO";

    @VisibleForTesting
    public static PendingIntentProvider getRemoveAllIncognitoTabsIntent(Context context) {
        Intent intent = new Intent(context, IncognitoNotificationService.class);
        intent.setAction(ACTION_CLOSE_ALL_INCOGNITO);
        return PendingIntentProvider.getService(
                context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT, IncognitoTabHostUtils::closeAllIncognitoTabs);

        boolean clearedIncognito = IncognitoTabPersistence.deleteIncognitoStateFiles();
        RecordHistogram.recordBooleanHistogram(
                "Android.IncognitoNotification.FileNotDeleted", !clearedIncognito);

        // If we failed clearing all of the incognito tabs, then do not dismiss the notification.
        if (!clearedIncognito) return;

        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (IncognitoTabHostUtils.doIncognitoTabsExist()) {
                        assert false : "Not all incognito tabs closed as expected";
                        return;
                    }
                    IncognitoNotificationManager.dismissIncognitoNotification();

                    if (BrowserStartupController.getInstance().isFullBrowserStarted()) {
                        if (ProfileManager.getLastUsedRegularProfile().hasPrimaryOTRProfile()) {
                            ProfileManager.destroyWhenAppropriate(
                                    ProfileManager.getLastUsedRegularProfile()
                                            .getPrimaryOTRProfile(/* createIfNeeded= */ false));
                        }
                    }
                });

        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Now ensure that the snapshots in recents are all cleared for Tabbed
                    // activities to remove any trace of incognito mode.
                    removeNonVisibleChromeTabbedRecentEntries();
                });
    }

    private void removeNonVisibleChromeTabbedRecentEntries() {
        Set<Integer> visibleTaskIds = getTaskIdsForVisibleActivities();
        HashSet<String> componentNames =
                new HashSet<>(ChromeTabbedActivity.TABBED_MODE_COMPONENT_NAMES);
        // It is not easily possible to distinguish between tasks sitting on top of
        // ChromeLauncherActivity, so we treat them all as likely ChromeTabbedActivities and
        // close them to be on the cautious side of things.
        componentNames.add(ChromeLauncherActivity.class.getName());
        Set<Pair<AppTask, RecentTaskInfo>> matchingTasks =
                AndroidTaskUtils.getRecentAppTasksMatchingComponentNames(
                        ContextUtils.getApplicationContext(), componentNames);

        for (Pair<AppTask, RecentTaskInfo> pair : matchingTasks) {
            RecentTaskInfo info = pair.second;
            if (!visibleTaskIds.contains(info.id)) {
                AppTask task = pair.first;
                task.finishAndRemoveTask();
            }
        }
    }

    private Set<Integer> getTaskIdsForVisibleActivities() {
        Set<Integer> visibleTaskIds = new HashSet<>();
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            int activityState = ApplicationStatus.getStateForActivity(activity);
            if (activityState != ActivityState.STOPPED
                    && activityState != ActivityState.DESTROYED) {
                visibleTaskIds.add(activity.getTaskId());
            }
        }
        return visibleTaskIds;
    }
}
