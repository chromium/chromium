// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for launching Chrome. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LauncherActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Context mContext;
    private static final long DEVICE_STARTUP_TIMEOUT_MS = 15000L;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @SmallTest
    public void testLaunchWithUrlNoScheme() {
        // Prepare intent
        final String intentUrl = "www.google.com";
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentUrl));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        final Activity startedActivity = tryLaunchingChrome(intent);
        final Intent activityIntent = startedActivity.getIntent();
        Assert.assertEquals(intentUrl, activityIntent.getDataString());
    }

    @Test
    @SmallTest
    public void testDoesNotCrashWithBadParcel() {
        // Prepare bad intent
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.google.com"));
        final Parcel parcel = Parcel.obtain();
        // Force unparcelling within ChromeLauncherActivity. Writing and reading from a parcel will
        // simulate being parcelled by another application, and thus cause unmarshalling when
        // Chrome tries reading an extra the next time.
        intent.writeToParcel(parcel, 0);
        intent.readFromParcel(parcel);
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra("BadExtra", new InvalidParcelable());

        final Activity startedActivity = tryLaunchingChrome(intent);
        final Intent activityIntent = startedActivity.getIntent();
        Assert.assertEquals("Data was not preserved", intent.getData(), activityIntent.getData());
        Assert.assertEquals(
                "Action was not preserved", intent.getAction(), activityIntent.getAction());
    }

    @Test
    @SmallTest
    public void testDoesNotCrashWithNoUriInViewIntent() {
        // Prepare intent
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Could crash after the activity is created, wait for the tab to stop loading.
        final ChromeActivity activity = (ChromeActivity) tryLaunchingChrome(intent);
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = activity.getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    Criteria.checkThat(tab.isLoading(), Matchers.is(false));
                },
                DEVICE_STARTUP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private Activity tryLaunchingChrome(final Intent intent) {
        mContext.startActivity(intent);

        // Check that ChromeLauncher Activity successfully launched
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getStateForApplication(),
                            Matchers.is(ApplicationState.HAS_RUNNING_ACTIVITIES));
                });

        // Check that Chrome proper was successfully launched as a follow-up
        final AtomicReference<Activity> launchedActivity = new AtomicReference<>();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final List<Activity> activities = ApplicationStatus.getRunningActivities();
                    Criteria.checkThat(activities.size(), Matchers.is(1));
                    launchedActivity.set(activities.get(0));
                    Criteria.checkThat(
                            launchedActivity.get(), Matchers.instanceOf(ChromeActivity.class));
                },
                DEVICE_STARTUP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return launchedActivity.get();
    }

    /**
     * This Parcelable does not adhere to the form standards of a well formed Parcelable and will
     * thus cause a BadParcelableException. The lint suppression is needed since it detects that
     * this will throw a BadParcelableException.
     */
    @SuppressLint("ParcelCreator")
    @SuppressWarnings("ParcelableCreator")
    private static class InvalidParcelable implements Parcelable {
        @Override
        public void writeToParcel(Parcel parcel, int params) {}

        @Override
        public int describeContents() {
            return 0;
        }
    }
}
