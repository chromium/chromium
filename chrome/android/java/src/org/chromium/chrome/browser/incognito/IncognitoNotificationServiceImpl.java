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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.base.SplitCompatIntentService;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.HashSet;
import java.util.Set;

/** Service that handles the action of clicking on the incognito notification. */
@NullMarked
public class IncognitoNotificationServiceImpl extends SplitCompatIntentService.Impl {
    private static final String ACTION_CLOSE_ALL_INCOGNITO =
            "com.google.android.apps.chrome.incognito.CLOSE_ALL_INCOGNITO";
    private static final int INVALID_TASK_ID = -1;

    @VisibleForTesting
    public static PendingIntentProvider getRemoveAllIncognitoTabsIntent(Context context) {
        Intent intent = new Intent(context, IncognitoNotificationService.class);
        intent.setAction(ACTION_CLOSE_ALL_INCOGNITO);
        return PendingIntentProvider.getService(
                context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    @Override
    protected void onHandleIntent(@Nullable Intent intent) {
        // Capture tasks whose live activities are exclusively regular-only tabbed windows before
        // closing incognito tabs, so a non-regular activity sharing a task cannot be finished and
        // destroyed before the recents sweep runs. Tasks with no live activity in ApplicationStatus
        // (e.g. after background process death) intentionally remain excluded so recents entries
        // that might show incognito content fail closed.
        Set<Integer> regularOnlyTaskIds =
                PostTask.runSynchronously(
                        TaskTraits.UI_DEFAULT, this::getRegularOnlyTaskIdsAndCloseIncognitoTabs);

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

                    if (BrowserStartupController.getInstance().isFullBrowserStarted()
                            && !IncognitoTabHostUtils.isIncognitoTabModelActive()) {
                        Profile otrProfile =
                                ProfileManager.getLastUsedRegularProfile()
                                        .getPrimaryOtrProfile(/* createIfNeeded= */ false);
                        if (otrProfile != null && !otrProfile.shutdownStarted()) {
                            ProfileManager.destroyWhenAppropriate(otrProfile);
                        }
                    }
                });

        // Now ensure that the snapshots in recents that may show incognito mode are
        // cleared to remove any trace of incognito mode.
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> removeRecentEntriesThatMayShowIncognito(regularOnlyTaskIds));
    }

    private Set<Integer> getRegularOnlyTaskIdsAndCloseIncognitoTabs() {
        Set<Integer> regularTaskIds = new HashSet<>();
        Set<Integer> nonRegularTaskIds = new HashSet<>();
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (ApplicationStatus.getStateForActivity(activity) == ActivityState.DESTROYED) {
                continue;
            }
            int taskId = activity.getTaskId();
            if (taskId == INVALID_TASK_ID) {
                continue;
            }
            if (isRegularOnlyTabbedActivity(activity)) {
                regularTaskIds.add(taskId);
            } else {
                nonRegularTaskIds.add(taskId);
            }
        }
        regularTaskIds.removeAll(nonRegularTaskIds);
        IncognitoTabHostUtils.closeAllIncognitoTabs();
        return regularTaskIds;
    }

    private static boolean isRegularOnlyTabbedActivity(Activity activity) {
        if (!(activity instanceof ChromeTabbedActivity cta)) {
            return false;
        }
        if (cta.isActivityFinishingOrDestroyed()) {
            return false;
        }
        if (cta.getSupportedProfileType() != SupportedProfileType.REGULAR) {
            return false;
        }
        if (!cta.areTabModelsInitialized()) {
            return true;
        }
        return cta.getTabModelSelector().getModel(/* incognito= */ true).getCount() == 0;
    }

    private void removeRecentEntriesThatMayShowIncognito(Set<Integer> regularOnlyTaskIds) {
        Set<Integer> taskIdsToKeep = getTaskIdsForVisibleActivities();
        taskIdsToKeep.addAll(regularOnlyTaskIds);
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
            if (info.id == INVALID_TASK_ID || !taskIdsToKeep.contains(info.id)) {
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
                int taskId = activity.getTaskId();
                if (taskId != INVALID_TASK_ID) {
                    visibleTaskIds.add(taskId);
                }
            }
        }
        return visibleTaskIds;
    }
}
