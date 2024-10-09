// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.util.Pair;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.util.HashSet;
import java.util.Set;

/** Operations that need to be executed on startup for incognito mode. */
public class IncognitoStartup {
    public static void onResumeWithNative(
            ProfileProvider profileProvider,
            CookiesFetcher cookiesFetcher,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            Set<String> componentNames) {
        if (profileProvider.hasOffTheRecordProfile()
                && shouldDestroyIncognitoProfileOnStartup(
                        tabModelSelectorSupplier.get().getCurrentModel().isIncognito(),
                        componentNames)) {
            ProfileManager.destroyWhenAppropriate(profileProvider.getOffTheRecordProfile(false));
        } else {
            cookiesFetcher.restoreCookies(CallbackUtils.emptyRunnable());
        }
    }

    /**
     * Determine whether the incognito profile needs to be destroyed as part of startup. This is
     * only needed on L+ when it is possible to swipe away tasks from Android recents without
     * killing the process. When this occurs, the normal incognito profile shutdown does not happen,
     * which can leave behind incognito cookies from an existing session.
     */
    @SuppressLint("NewApi")
    private static boolean shouldDestroyIncognitoProfileOnStartup(
            boolean selectedTabModelIsIncognito, Set<String> componentNames) {
        Set<Pair<AppTask, RecentTaskInfo>> tabbedModeTasks =
                AndroidTaskUtils.getRecentAppTasksMatchingComponentNames(
                        ContextUtils.getApplicationContext(), componentNames);

        Set<Integer> tabbedModeTaskIds = new HashSet<>();
        for (Pair<AppTask, RecentTaskInfo> pair : tabbedModeTasks) {
            tabbedModeTaskIds.add(pair.second.id);
        }

        if (tabbedModeTaskIds.size() == 0) {
            return true;
        }

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            tabbedModeTaskIds.remove(activity.getTaskId());
        }

        // If all tabbed mode tasks listed in Android recents are alive, check to see if
        // any incognito tabs exist and the current tab model isn't incognito. If so, we should
        // destroy the incognito profile; otherwise it's not safe to do so yet.
        if (tabbedModeTaskIds.size() == 0) {
            return !(IncognitoTabHostUtils.doIncognitoTabsExist() || selectedTabModelIsIncognito);
        }

        // In this case, we have tabbed mode activities listed in recents that do not have an
        // active running activity associated with them.  We can not accurately get an incognito
        // tab count as we do not know if any incognito tabs are associated with the yet unrestored
        // tabbed mode.  Thus we do not proactively destroy the incognito profile.
        return false;
    }
}
