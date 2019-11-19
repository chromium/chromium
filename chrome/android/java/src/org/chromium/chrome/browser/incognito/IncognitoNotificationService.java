// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.HashSet;
import java.util.Set;

/**
 * Service that handles the action of clicking on the incognito notification.
 */
public class IncognitoNotificationService extends IntentService {

    private static final String TAG = "incognito_notification";

    private static final String ACTION_CLOSE_ALL_INCOGNITO =
            "com.google.android.apps.chrome.incognito.CLOSE_ALL_INCOGNITO";

    @VisibleForTesting
    public static PendingIntentProvider getRemoveAllIncognitoTabsIntent(Context context) {
        Intent intent = new Intent(context, IncognitoNotificationService.class);
        intent.setAction(ACTION_CLOSE_ALL_INCOGNITO);
        return PendingIntentProvider.getService(
                context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /** Empty public constructor needed by Android. */
    public IncognitoNotificationService() {
        super(TAG);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        PostTask.runSynchronously(
                UiThreadTaskTraits.DEFAULT, IncognitoUtils::closeAllIncognitoTabs);

        boolean clearedIncognito = IncognitoUtils.deleteIncognitoStateFiles();

        // If we failed clearing all of the incognito tabs, then do not dismiss the notification.
        if (!clearedIncognito) return;

        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
            if (IncognitoUtils.doIncognitoTabsExist()) {
                assert false : "Not all incognito tabs closed as expected";
                return;
            }
            IncognitoNotificationManager.dismissIncognitoNotification();

            if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isFullBrowserStarted()) {
                if (Profile.getLastUsedProfile().hasOffTheRecordProfile()) {
                    Profile.getLastUsedProfile().getOffTheRecordProfile().destroyWhenAppropriate();
                }
            }
        });

        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
            // Now ensure that the snapshots in recents are all cleared for Tabbed activities
            // to remove any trace of incognito mode.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                focusChromeIfNecessary();
            } else {
                removeNonVisibleChromeTabbedRecentEntries();
            }
        });
    }

    private void focusChromeIfNecessary() {
        Set<Integer> visibleTaskIds = getTaskIdsForVisibleActivities();
        int tabbedTaskId = -1;

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity instanceof ChromeTabbedActivity) {
                tabbedTaskId = activity.getTaskId();
                break;
            }
        }

        // If the task containing the tabbed activity is visible, then do nothing as there is no
        // snapshot that would need to be regenerated.
        if (visibleTaskIds.contains(tabbedTaskId)) return;

        Context context = ContextUtils.getApplicationContext();
        Intent startIntent = new Intent(Intent.ACTION_MAIN);
        startIntent.setPackage(context.getPackageName());
        startIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(startIntent);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void removeNonVisibleChromeTabbedRecentEntries() {
        Set<Integer> visibleTaskIds = getTaskIdsForVisibleActivities();

        Context context = ContextUtils.getApplicationContext();
        ActivityManager manager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);

        for (AppTask task : manager.getAppTasks()) {
            RecentTaskInfo info = AndroidTaskUtils.getTaskInfoFromTask(task);
            if (info == null) continue;
            String componentName = AndroidTaskUtils.getTaskComponentName(task);

            // It is not easily possible to distinguish between tasks sitting on top of
            // ChromeLauncherActivity, so we treat them all as likely ChromeTabbedActivities and
            // close them to be on the cautious side of things.
            if ((ChromeTabbedActivity.isTabbedModeComponentName(componentName)
                        || TextUtils.equals(componentName, ChromeLauncherActivity.class.getName()))
                    && !visibleTaskIds.contains(info.id)) {
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
