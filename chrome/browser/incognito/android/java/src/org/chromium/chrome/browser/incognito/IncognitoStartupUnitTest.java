// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerJni;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link IncognitoStartup}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoStartupUnitTest {
    private static final String TABBED_COMPONENT_NAME =
            "org.chromium.chrome.browser.TabbedActivity";
    private static final Set<String> TABBED_COMPONENT_NAMES = Set.of(TABBED_COMPONENT_NAME);
    private static final int TABBED_TASK_ID = 17;

    /** Minimal Activity whose task id can be controlled, tracked via {@link ApplicationStatus}. */
    private static class FakeTabbedActivity extends Activity {
        private final int mTaskId;

        FakeTabbedActivity(int taskId) {
            mTaskId = taskId;
        }

        @Override
        public int getTaskId() {
            return mTaskId;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mOtrProfile;
    @Mock private CookiesFetcher mCookiesFetcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mCurrentTabModel;
    @Mock private ProfileManager.Natives mProfileManagerJni;

    private SettableMonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final List<Activity> mRunningActivities = new ArrayList<>();

    @Before
    public void setUp() {
        ProfileManagerJni.setInstanceForTesting(mProfileManagerJni);
        IncognitoTabHostRegistry.getInstance().clearForTesting();

        doReturn(mOtrProfile).when(mProfileProvider).getOffTheRecordProfile();
        doReturn(mCurrentTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(false).when(mCurrentTabModel).isIncognito();
        mTabModelSelectorSupplier = ObservableSuppliers.createMonotonic();
        mTabModelSelectorSupplier.set(mTabModelSelector);
    }

    @After
    public void tearDown() {
        for (Activity activity : mRunningActivities) {
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
        }
        mRunningActivities.clear();
        IncognitoTabHostRegistry.getInstance().clearForTesting();
    }

    private void onResumeWithNative() {
        IncognitoStartup.onResumeWithNative(
                mProfileProvider,
                mCookiesFetcher,
                mTabModelSelectorSupplier,
                TABBED_COMPONENT_NAMES);
    }

    /** Lists tabbed-mode tasks with the given ids in Android recents. */
    private void setRecentTabbedTasks(int... taskIds) {
        List<AppTask> appTasks = new ArrayList<>();
        for (int taskId : taskIds) {
            RecentTaskInfo taskInfo = new RecentTaskInfo();
            taskInfo.id = taskId;
            taskInfo.taskId = taskId;
            taskInfo.baseIntent =
                    new Intent()
                            .setComponent(
                                    new ComponentName(
                                            ContextUtils.getApplicationContext(),
                                            TABBED_COMPONENT_NAME));
            AppTask appTask = mock(AppTask.class);
            doReturn(taskInfo).when(appTask).getTaskInfo();
            appTasks.add(appTask);
        }
        ActivityManager activityManager =
                (ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        shadowOf(activityManager).setAppTasks(appTasks);
    }

    /** Registers a running Activity in the given task with {@link ApplicationStatus}. */
    private void addRunningActivityInTask(int taskId) {
        Activity activity = new FakeTabbedActivity(taskId);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        mRunningActivities.add(activity);
    }

    private static void registerHost(boolean hasIncognitoTabs, boolean isActiveModel) {
        IncognitoTabHost host = mock(IncognitoTabHost.class);
        doReturn(hasIncognitoTabs).when(host).hasIncognitoTabs();
        doReturn(isActiveModel).when(host).isActiveModel();
        IncognitoTabHostRegistry.getInstance().register(host);
    }

    /** All tabbed tasks in recents have a live Activity; no incognito tabs anywhere. */
    private void setUpAllTabbedTasksAliveWithoutIncognito() {
        setRecentTabbedTasks(TABBED_TASK_ID);
        addRunningActivityInTask(TABBED_TASK_ID);
    }

    @Test
    public void testNoOtrProfile_restoresCookies() {
        doReturn(null).when(mProfileProvider).getOffTheRecordProfile();
        setUpAllTabbedTasksAliveWithoutIncognito();

        onResumeWithNative();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
        verify(mCookiesFetcher).restoreCookies(any());
    }

    @Test
    public void testNoTabbedTasksInRecents_destroysProfile() {
        setRecentTabbedTasks();
        registerHost(/* hasIncognitoTabs= */ true, /* isActiveModel= */ true);

        onResumeWithNative();

        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
        verify(mCookiesFetcher, never()).restoreCookies(any());
    }

    @Test
    public void testTabbedTaskWithoutRunningActivity_doesNotDestroyProfile() {
        // A tabbed task is listed in recents but not yet restored: its incognito tabs are unknown.
        setRecentTabbedTasks(TABBED_TASK_ID);

        onResumeWithNative();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
        verify(mCookiesFetcher).restoreCookies(any());
    }

    @Test
    public void testAllTabbedTasksAlive_noIncognitoTabs_destroysProfile() {
        setUpAllTabbedTasksAliveWithoutIncognito();
        registerHost(/* hasIncognitoTabs= */ false, /* isActiveModel= */ false);

        onResumeWithNative();

        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfile);
        verify(mCookiesFetcher, never()).restoreCookies(any());
    }

    @Test
    public void testAllTabbedTasksAlive_hostHasIncognitoTabs_doesNotDestroyProfile() {
        setUpAllTabbedTasksAliveWithoutIncognito();
        registerHost(/* hasIncognitoTabs= */ true, /* isActiveModel= */ false);

        onResumeWithNative();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
        verify(mCookiesFetcher).restoreCookies(any());
    }

    @Test
    public void testAllTabbedTasksAlive_hostIsActiveModelWithoutTabs_doesNotDestroyProfile() {
        setUpAllTabbedTasksAliveWithoutIncognito();
        // E.g. a popup Custom Tab that is finishing but not yet destroyed.
        registerHost(/* hasIncognitoTabs= */ false, /* isActiveModel= */ true);

        onResumeWithNative();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
        verify(mCookiesFetcher).restoreCookies(any());
    }

    @Test
    public void testAllTabbedTasksAlive_selectedModelIsIncognito_doesNotDestroyProfile() {
        setUpAllTabbedTasksAliveWithoutIncognito();
        doReturn(true).when(mCurrentTabModel).isIncognito();

        onResumeWithNative();

        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
        verify(mCookiesFetcher).restoreCookies(any());
    }
}
