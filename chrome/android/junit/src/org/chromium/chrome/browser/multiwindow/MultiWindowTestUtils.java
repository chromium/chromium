// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import org.mockito.Mockito;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Test util methods for multi-window/instance support */
public class MultiWindowTestUtils {
    /**
     * Create a new instance information.
     * @param instanceId Instance (aka window) ID.
     * @param url URL for the active tab.
     * @param tabCount The number of tabs in the instance.
     * @param taskId ID of the task the activity instance runs in.
     */
    public static void createInstance(int instanceId, String url, int tabCount, int taskId) {
        MultiInstanceManagerApi31.writeUrl(instanceId, url);
        MultiInstanceManagerApi31.writeLastAccessedTime(instanceId);
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeInt(MultiInstanceManagerApi31.tabCountKey(instanceId), tabCount);
        MultiInstanceManagerApi31.updateTaskMap(instanceId, taskId);
    }

    /** Clears all the instance information */
    public static void resetInstanceInfo() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_URL);
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME);
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT);
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
    }

    /** Enabled multi instance. */
    public static void enableMultiInstance() throws NameNotFoundException {
        Context applicationContext = Mockito.spy(ContextUtils.getApplicationContext());
        PackageManager packageManager = mock(PackageManager.class);
        when(applicationContext.getPackageManager()).thenReturn(packageManager);
        ActivityInfo activityInfo = mock(ActivityInfo.class);
        when(packageManager.getActivityInfo(any(), anyInt())).thenReturn(activityInfo);
        ContextUtils.initApplicationContextForTests(applicationContext);
        activityInfo.launchMode = ActivityInfo.LAUNCH_SINGLE_INSTANCE_PER_TASK;
    }
}
