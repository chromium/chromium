// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.not;

import static org.chromium.android_webview.test.common.crash.CrashInfoTest.createCrashInfo;
import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.IdRes;
import androidx.test.espresso.DataInteraction;
import androidx.test.filters.LargeTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assume;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.devui.CrashesListFragment;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.FileUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Date;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * UI tests for {@link CrashesListFragment}.
 */
@LargeTest
@RunWith(AwJUnit4ClassRunner.class)
public class CrashesListFragmentTest {
    private static final String FAKE_APP_PACKAGE_NAME = "com.test.some_package";
    private static final String CRASH_REPORT_BUTTON_TEXT = "File bug report";
    private static final String CRASH_UPLOAD_BUTTON_TEXT = "Upload this crash report";

    @Rule
    public ActivityTestRule mRule =
            new ActivityTestRule<MainActivity>(MainActivity.class, false, false);

    @After
    public void tearDown() {
        FileUtils.recursivelyDeleteFile(SystemWideCrashDirectories.getWebViewCrashDir(), null);
        FileUtils.recursivelyDeleteFile(SystemWideCrashDirectories.getWebViewCrashLogDir(), null);
    }

    private void launchCrashesFragment() {
        Intent intent = new Intent();
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_CRASHES);
        mRule.launchActivity(intent);
        onView(withId(R.id.fragment_crashes_list)).check(matches(isDisplayed()));
    }

    private static File createMinidumpFile(CrashInfo crashInfo) throws IOException {
        CrashFileManager crashFileManager =
                new CrashFileManager(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());
        File dir = crashFileManager.getCrashDirectory();
        dir.mkdirs();
        String suffix;
        switch (crashInfo.uploadState) {
            case UPLOADED:
                suffix = ".up";
                break;
            case SKIPPED:
                suffix = ".skipped";
                break;
            case PENDING_USER_REQUESTED:
                suffix = ".forced";
                break;
            default:
                suffix = ".dmp";
        }
        return File.createTempFile(
                "test_minidump", "-" + crashInfo.localId + suffix + ".try0", dir);
    }

    private static File appendUploadedEntryToLog(CrashInfo crashInfo) throws IOException {
        CrashFileManager crashFileManager =
                new CrashFileManager(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());
        File logFile = crashFileManager.getCrashUploadLogFile();
        logFile.getParentFile().mkdirs();
        FileWriter writer = new FileWriter(logFile, /* append= */ true);
        StringBuilder sb = new StringBuilder();
        sb.append(TimeUnit.MILLISECONDS.toSeconds(crashInfo.uploadTime));
        sb.append(",");
        sb.append(crashInfo.uploadId);
        sb.append(",");
        sb.append(crashInfo.localId);
        sb.append('\n');
        try {
            writer.write(sb.toString());
        } finally {
            writer.close();
        }

        return logFile;
    }

    private static File writeJsonLogFile(CrashInfo crashInfo) throws IOException {
        File dir = SystemWideCrashDirectories.getOrCreateWebViewCrashLogDir();
        File jsonFile = File.createTempFile("test_minidump-", crashInfo.localId + ".json", dir);
        FileWriter writer = new FileWriter(jsonFile);
        writer.write(crashInfo.serializeToJson());
        writer.close();
        return jsonFile;
    }

    private CallbackHelper getCrashListLoadedListener() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            final CallbackHelper helper = new CallbackHelper();
            CrashesListFragment.setCrashInfoLoadedListenerForTesting(
                    () -> { helper.notifyCalled(); });
            return helper;
        });
    }

    /**
     * Matches that a {@link ImageView} has the given {@link Drawable}.
     */
    private static Matcher<View> withDrawable(Drawable expectedDrawable) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof ImageView)) {
                    return false;
                }
                return drawableEquals(((ImageView) view).getDrawable(), expectedDrawable);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with Drawable");
            }
        };
    }

    /**
     * Matches that a {@link ImageView} has the given {@link Drawable}.
     *
     * @param expectedId the id resource for the given drawable
     */
    private static Matcher<View> withDrawable(@IdRes int expectedId) {
        return new TypeSafeMatcher<View>() {
            private Resources mResources =
                    InstrumentationRegistry.getTargetContext().getResources();

            @Override
            public boolean matchesSafely(View view) {
                Drawable expectedDrawable = mResources.getDrawable(expectedId);
                return withDrawable(expectedDrawable).matches(view);
            }

            @Override
            public void describeTo(Description description) {
                try {
                    description.appendText("with Drawable Id: ")
                            .appendText(mResources.getResourceName(expectedId));
                } catch (Resources.NotFoundException e) {
                    description.appendText("with Drawable Id (resource name not found): ")
                            .appendText(Integer.toString(expectedId));
                }
            }
        };
    }

    private static boolean drawableEquals(Drawable actualDrawable, Drawable expectedDrawable) {
        if (actualDrawable == null || expectedDrawable == null) {
            return false;
        }
        Bitmap actualBitmap = getBitmap(actualDrawable);
        Bitmap expectedBitmap = getBitmap(expectedDrawable);
        return actualBitmap.sameAs(expectedBitmap);
    }

    // Convert a drawable to a Bitmap for comparison.
    private static Bitmap getBitmap(Drawable drawable) {
        Bitmap bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    /**
     * Check that the given crash item header shows the correct package name, capture date and icon
     * for the given {@code crashInfo}.
     *
     * @param {@link DataInteraction} represents the crash item header.
     * @param {@link CrashInfo} to match.
     * @return the same {@code headerDataInteraction} passed for the convenience of chaining.
     */
    private static DataInteraction checkUnknownPackageCrashItemHeader(
            DataInteraction headerDataInteraction, CrashInfo crashInfo) {
        String captureDate = new Date(crashInfo.captureTime).toString();
        headerDataInteraction.onChildView(withId(android.R.id.text1))
                .check(matches(withText(FAKE_APP_PACKAGE_NAME)));
        headerDataInteraction.onChildView(withId(android.R.id.text2))
                .check(matches(withText(captureDate)));
        // There should not be an app with FAKE_APP_PACKAGE_NAME so system default icon should be
        // shown.
        headerDataInteraction.onChildView(withId(R.id.crash_package_icon))
                .check(matches(withDrawable(android.R.drawable.sym_def_app_icon)));

        return headerDataInteraction;
    }

    /**
     * Check that the given crash item body shows the correct uploadState, uploadId and uploadDate.
     *
     * @param {@link DataInteraction} represents the crash item body.
     * @param {@link CrashInfo} to match its upload status.
     * @return the same {@code headerDataInteraction} passed for the convenience of chaining.
     */
    private static DataInteraction checkCrashItemUploadStatus(
            DataInteraction bodyDataInteraction, CrashInfo crashInfo) {
        DataInteraction uploadStatusDataInteraction =
                bodyDataInteraction.onChildView(withId(R.id.upload_status));
        String uploadState = CrashesListFragment.uploadStateString(crashInfo.uploadState);
        uploadStatusDataInteraction.onChildView(withId(android.R.id.text1))
                .check(matches(withText(uploadState)));
        String uploadInfo = crashInfo.uploadState == UploadState.UPLOADED
                ? new Date(crashInfo.uploadTime).toString() + "\nID: " + crashInfo.uploadId
                : "";
        uploadStatusDataInteraction.onChildView(withId(android.R.id.text2))
                .check(matches(withText(uploadInfo)));

        return bodyDataInteraction;
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_uploaded() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo = createCrashInfo("123456", systemTime, "0abcde123456",
                systemTime + 1000, FAKE_APP_PACKAGE_NAME, UploadState.UPLOADED);

        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());
        assertThat("upload log file should exist", appendUploadedEntryToLog(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkUnknownPackageCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        bodyDataInteraction.onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)));
        bodyDataInteraction.onChildView(withId(R.id.crash_upload_button))
                .check(matches(not(isDisplayed())));
        bodyDataInteraction.onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_pending() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo = createCrashInfo(
                "123456", systemTime, null, -1, FAKE_APP_PACKAGE_NAME, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkUnknownPackageCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        bodyDataInteraction.onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)))
                .check(matches(not(isEnabled())));
        bodyDataInteraction.onChildView(withId(R.id.crash_upload_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_UPLOAD_BUTTON_TEXT)))
                .check(matches(isEnabled()));
        bodyDataInteraction.onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_pendingUserRequest() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo = createCrashInfo("123456", systemTime, null, -1, FAKE_APP_PACKAGE_NAME,
                UploadState.PENDING_USER_REQUESTED);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkUnknownPackageCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        bodyDataInteraction.onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)))
                .check(matches(not(isEnabled())))
                .perform(click());
        bodyDataInteraction.onChildView(withId(R.id.crash_upload_button))
                .check(matches(not(isDisplayed())));
        bodyDataInteraction.onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_skipped() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo = createCrashInfo(
                "123456", systemTime, null, -1, FAKE_APP_PACKAGE_NAME, UploadState.SKIPPED);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkUnknownPackageCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        bodyDataInteraction.onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)))
                .check(matches(not(isEnabled())));
        bodyDataInteraction.onChildView(withId(R.id.crash_upload_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_UPLOAD_BUTTON_TEXT)))
                .check(matches(isEnabled()));
        bodyDataInteraction.onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    // Test when a crash has a known package name that can be found using PackageManager
    public void testInstalledPackageInfo() throws Throwable {
        Context context = InstrumentationRegistry.getTargetContext();
        PackageManager packageManager = context.getPackageManager();
        // Use the system settings package as a fake app where a crash happened because it's more
        // likely to be available on every device. If it's not found, skip the test.
        final String appPackageName = "com.android.settings";
        ApplicationInfo appInfo;
        try {
            appInfo = packageManager.getApplicationInfo(appPackageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            appInfo = null;
        }
        Assume.assumeNotNull(
                "This test assumes \"com.android.settings\" package is available", appInfo);

        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo = createCrashInfo(
                "123456", systemTime, null, -1, appPackageName, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        DataInteraction headerDataInteraction = onData(anything()).atPosition(0);
        headerDataInteraction.onChildView(withId(android.R.id.text1))
                .check(matches(withText(appPackageName)));
        headerDataInteraction.onChildView(withId(R.id.crash_package_icon))
                .check(matches(withDrawable(packageManager.getApplicationIcon(appInfo))));
    }

    @Test
    @Feature({"AndroidWebView"})
    // Test when app package name field is missing in the crash info.
    public void testMissingPackageInfo() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfo("123456", systemTime, null, -1, null, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        DataInteraction headerDataInteraction = onData(anything()).atPosition(0);
        headerDataInteraction.onChildView(withId(android.R.id.text1))
                .check(matches(withText("unknown app")));
        headerDataInteraction.onChildView(withId(R.id.crash_package_icon))
                .check(matches(withDrawable(android.R.drawable.sym_def_app_icon)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testMaxNumberOfCrashes() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        final int crashReportsNum = CrashesListFragment.MAX_CRASHES_NUMBER + 10;
        CrashInfo[] crashInfo = new CrashInfo[crashReportsNum];
        for (int i = 0; i < crashReportsNum; ++i) {
            // Set capture time with an arbitrary chosen 2 second difference to make sure crashes
            // are shown in descending order with most recent crash first.
            crashInfo[i] = createCrashInfo("abcd" + Integer.toString(i), systemTime + i * 2000,
                    null, -1, FAKE_APP_PACKAGE_NAME, UploadState.PENDING);

            assertThat(
                    "temp minidump file should exist", createMinidumpFile(crashInfo[i]).exists());
            assertThat("temp json log file should exist", writeJsonLogFile(crashInfo[i]).exists());
        }

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list))
                .check(matches(withCount(CrashesListFragment.MAX_CRASHES_NUMBER)));
        // Check that only the most recent MAX_CRASHES_NUMBER crashes are shown.
        for (int i = 0; i < CrashesListFragment.MAX_CRASHES_NUMBER; ++i) {
            // Crashes should be shown with the most recent first, i.e the reverse of the order
            // they are initialized at.
            checkUnknownPackageCrashItemHeader(
                    onData(anything()).atPosition(i), crashInfo[crashReportsNum - i - 1]);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testHideCrashButton_uploaded() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo = createCrashInfo("123456", systemTime, "0abcde123456",
                systemTime + 1000, FAKE_APP_PACKAGE_NAME, UploadState.UPLOADED);

        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());
        assertThat("upload log file should exist", appendUploadedEntryToLog(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkUnknownPackageCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);

        crashListLoadInitCount = helper.getCallCount();

        bodyDataInteraction.onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)))
                .perform(click());

        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(0)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testHideCrashButton_pending() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo = createCrashInfo(
                "123456", systemTime, null, -1, FAKE_APP_PACKAGE_NAME, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkUnknownPackageCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);

        crashListLoadInitCount = helper.getCallCount();

        bodyDataInteraction.onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)))
                .perform(click());

        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(0)));
    }
}
