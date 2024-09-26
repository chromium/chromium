// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.assertNoUnverifiedIntents;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.android_webview.nonembedded.crash.CrashInfo.createCrashInfoForTesting;
import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.getClipBoardTextOnUiThread;
import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.setClipBoardTextOnUiThread;
import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.IdRes;
import androidx.test.espresso.DataInteraction;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.devui.CrashesListFragment;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.devui.WebViewPackageError;
import org.chromium.android_webview.devui.util.CrashBugUrlFactory;
import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.nonembedded.crash.CrashInfo.UploadState;
import org.chromium.android_webview.nonembedded.crash.CrashUploadUtil;
import org.chromium.android_webview.nonembedded.crash.CrashUploadUtil.CrashUploadDelegate;
import org.chromium.android_webview.nonembedded.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/** UI tests for {@link CrashesListFragment}. */
@LargeTest
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Batching causes test failures.")
public class CrashesListFragmentTest {
    private static final String FAKE_APP_PACKAGE_NAME = "com.test.some_package";
    private static final String CRASH_REPORT_BUTTON_TEXT = "File bug report";
    private static final String CRASH_UPLOAD_BUTTON_TEXT = "Upload this crash report";

    @Rule
    public BaseActivityTestRule<MainActivity> mRule =
            new BaseActivityTestRule<>(MainActivity.class);

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        // Mark popup permission as already requested to suppress the popup
        MainActivity.markPopupPermissionRequestedInPrefsForTesting();
    }

    @After
    public void tearDown() {
        FileUtils.recursivelyDeleteFile(SystemWideCrashDirectories.getWebViewCrashDir(), null);
        FileUtils.recursivelyDeleteFile(SystemWideCrashDirectories.getWebViewCrashLogDir(), null);

        // Activity is launched, i.e the test is not skipped.
        if (mRule.getActivity() != null) {
            // Tests are responsible for verifying every Intent they trigger.
            assertNoUnverifiedIntents();
            Intents.release();
        }
    }

    private void launchCrashesFragment() {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), MainActivity.class);
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_CRASHES);
        mRule.launchActivity(intent);
        onView(withId(R.id.fragment_crashes_list)).check(matches(isDisplayed()));

        // Only start recording intents after launching the MainActivity.
        Intents.init();

        // Stub all external intents, to avoid launching other apps (ex. system browser), has to be
        // done after launching the activity.
        intending(not(IntentMatchers.isInternal()))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));
    }

    private static File createMinidumpFile(CrashInfo crashInfo) throws IOException {
        CrashFileManager crashFileManager =
                new CrashFileManager(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());
        File dir = crashFileManager.getCrashDirectory();
        dir.mkdirs();
        String suffix =
                switch (crashInfo.uploadState) {
                    case UPLOADED -> ".up";
                    case SKIPPED -> ".skipped";
                    case PENDING_USER_REQUESTED -> ".forced";
                    default -> ".dmp";
                };
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
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final CallbackHelper helper = new CallbackHelper();
                    CrashesListFragment.setCrashInfoLoadedListenerForTesting(helper::notifyCalled);
                    return helper;
                });
    }

    /** Matches that a {@link ImageView} has the given {@link Drawable}. */
    private static Matcher<View> withDrawable(Drawable expectedDrawable) {
        return new TypeSafeMatcher<>() {
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
        return new TypeSafeMatcher<>() {
            private final Resources mResources =
                    ContextUtils.getApplicationContext().getResources();

            @Override
            public boolean matchesSafely(View view) {
                Drawable expectedDrawable = view.getContext().getDrawable(expectedId);
                return withDrawable(expectedDrawable).matches(view);
            }

            @Override
            public void describeTo(Description description) {
                try {
                    description
                            .appendText("with Drawable Id: ")
                            .appendText(mResources.getResourceName(expectedId));
                } catch (Resources.NotFoundException e) {
                    description
                            .appendText("with Drawable Id (resource name not found): ")
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
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
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
        return checkPackageCrashItemHeader(headerDataInteraction, crashInfo, FAKE_APP_PACKAGE_NAME);
    }

    /**
     * Check that the given crash item header shows the "unknown app" package name, capture date and
     * icon for the given {@code crashInfo}.
     *
     * @param {@link DataInteraction} represents the crash item header.
     * @param {@link CrashInfo} to match.
     * @return the same {@code headerDataInteraction} passed for the convenience of chaining.
     */
    private static DataInteraction checkMissingPackageInfoCrashItemHeader(
            DataInteraction headerDataInteraction, CrashInfo crashInfo) {
        return checkPackageCrashItemHeader(headerDataInteraction, crashInfo, "unknown app");
    }

    /**
     * Check that the given crash item header shows the given package name, capture date and
     * icon for the given {@code crashInfo}.
     *
     * @param {@link DataInteraction} represents the crash item header.
     * @param {@link CrashInfo} to match.
     * @param packageName to match.
     * @return the same {@code headerDataInteraction} passed for the convenience of chaining.
     */
    private static DataInteraction checkPackageCrashItemHeader(
            DataInteraction headerDataInteraction, CrashInfo crashInfo, String packageName) {
        String captureDate = new Date(crashInfo.captureTime).toString();
        headerDataInteraction
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText(packageName)));
        headerDataInteraction
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(captureDate)));
        // There should not be an app with FAKE_APP_PACKAGE_NAME so system default icon should be
        // shown.
        headerDataInteraction
                .onChildView(withId(R.id.crash_package_icon))
                .check(matches(withDrawable(android.R.drawable.sym_def_app_icon)));

        return headerDataInteraction;
    }

    /**
     * Perform click on hide crash button by checking the required conditions for the button.
     *
     * @param {@link DataInteraction} represents the crash item body.
     */
    private static void clickHideCrashButton(DataInteraction bodyDataInteraction) {
        bodyDataInteraction
                .onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)))
                .perform(click());
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
        uploadStatusDataInteraction
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText(uploadState)));
        String uploadInfo =
                crashInfo.uploadState == UploadState.UPLOADED
                        ? new Date(crashInfo.uploadTime).toString() + "\nID: " + crashInfo.uploadId
                        : "";
        uploadStatusDataInteraction
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(uploadInfo)));

        return bodyDataInteraction;
    }

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private final boolean mCanUseGms;
        private final boolean mUserConsent;

        TestPlatformServiceBridge(boolean canUseGms, boolean userConsent) {
            mCanUseGms = canUseGms;
            mUserConsent = userConsent;
        }

        @Override
        public boolean canUseGms() {
            return mCanUseGms;
        }

        @Override
        public void queryMetricsSetting(Callback<Boolean> callback) {
            callback.onResult(mUserConsent);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_uploaded() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456",
                        systemTime,
                        "0abcde123456",
                        systemTime + 1000,
                        FAKE_APP_PACKAGE_NAME,
                        UploadState.UPLOADED);

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

        bodyDataInteraction
                .onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)));
        bodyDataInteraction
                .onChildView(withId(R.id.crash_upload_button))
                .check(matches(not(isDisplayed())));
        bodyDataInteraction
                .onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testOpenBugReportCrash() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456",
                        systemTime,
                        "0abcde123456",
                        systemTime + 1000,
                        FAKE_APP_PACKAGE_NAME,
                        UploadState.UPLOADED);

        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());
        assertThat("upload log file should exist", appendUploadedEntryToLog(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));
        // Click the header to expand the list item.
        onData(anything()).atPosition(0).perform(click());
        // The body is considered item#2 in the list view after expansion.
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        onData(anything())
                .atPosition(1)
                .onChildView(withId(R.id.crash_report_button))
                .perform(click());
        onView(withText(CrashesListFragment.CRASH_BUG_DIALOG_MESSAGE))
                .check(matches(isDisplayed()));
        // button2 is the AlertDialog negative button id.
        onView(withId(android.R.id.button2)).check(matches(withText("Dismiss"))).perform(click());
        onView(withText(CrashesListFragment.CRASH_BUG_DIALOG_MESSAGE)).check(doesNotExist());
        // Verify that no intents are sent out.
        Intents.times(0);

        onData(anything())
                .atPosition(1)
                .onChildView(withId(R.id.crash_report_button))
                .perform(click());

        Intent expectedIntent = new CrashBugUrlFactory(crashInfo).getReportIntent();
        ActivityResult intentResult = new ActivityResult(Activity.RESULT_OK, new Intent());
        // Stub out the intent we expect to receive.
        intending(IntentMatchers.filterEquals(expectedIntent)).respondWith(intentResult);

        // button1 is the AlertDialog positive button id.
        onView(withId(android.R.id.button1))
                .check(matches(withText("Provide more info")))
                .perform(click());
        onView(withText(CrashesListFragment.CRASH_BUG_DIALOG_MESSAGE)).check(doesNotExist());
        intended(IntentMatchers.filterEquals(expectedIntent));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_pending() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
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

        bodyDataInteraction
                .onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)))
                .check(matches(not(isEnabled())));
        bodyDataInteraction
                .onChildView(withId(R.id.crash_upload_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_UPLOAD_BUTTON_TEXT)))
                .check(matches(isEnabled()));
        bodyDataInteraction
                .onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_pendingUserRequest() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456",
                        systemTime,
                        null,
                        -1,
                        FAKE_APP_PACKAGE_NAME,
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

        bodyDataInteraction
                .onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)))
                .check(matches(not(isEnabled())))
                .perform(click());
        bodyDataInteraction
                .onChildView(withId(R.id.crash_upload_button))
                .check(matches(not(isDisplayed())));
        bodyDataInteraction
                .onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testShowingSingleCrashReport_skipped() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
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

        bodyDataInteraction
                .onChildView(withId(R.id.crash_report_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_REPORT_BUTTON_TEXT)))
                .check(matches(not(isEnabled())));
        bodyDataInteraction
                .onChildView(withId(R.id.crash_upload_button))
                .check(matches(isDisplayed()))
                .check(matches(withText(CRASH_UPLOAD_BUTTON_TEXT)))
                .check(matches(isEnabled()));
        bodyDataInteraction
                .onChildView(withId(R.id.crash_hide_button))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()))
                .check(matches(withDrawable(R.drawable.ic_delete)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testForceUploadSkippedCrashReport_noWifi() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456", systemTime, null, -1, FAKE_APP_PACKAGE_NAME, UploadState.SKIPPED);

        File minidumpFile = createMinidumpFile(crashInfo);
        assertThat("temp minidump file should exist", minidumpFile.exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CrashUploadUtil.setCrashUploadDelegateForTesting(
                new CrashUploadDelegate() {
                    @Override
                    public void scheduleNewJob(Context context, boolean requiresUnmeteredNetwork) {}

                    @Override
                    public boolean isNetworkUnmetered(Context context) {
                        return false;
                    }
                });

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // click on the crash item header to expand
        onData(anything()).atPosition(0).perform(click());
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        // Firstly test clicking the upload button, and dismissing the dialog
        bodyDataInteraction.onChildView(withId(R.id.crash_upload_button)).perform(click());
        onView(withText(CrashesListFragment.NO_WIFI_DIALOG_MESSAGE)).check(matches(isDisplayed()));
        // button2 is the AlertDialog negative button id.
        onView(withId(android.R.id.button2)).check(matches(withText("Cancel"))).perform(click());
        // Check no changes in the view after dismissing the dialog
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);
        bodyDataInteraction
                .onChildView(withId(R.id.crash_upload_button))
                .check(matches(isDisplayed()));

        // Secondly test clicking the upload button, and proceeding with upload.
        crashListLoadInitCount = helper.getCallCount();
        bodyDataInteraction.onChildView(withId(R.id.crash_upload_button)).perform(click());
        onView(withText(CrashesListFragment.NO_WIFI_DIALOG_MESSAGE)).check(matches(isDisplayed()));
        // button1 is the AlertDialog positive button id.
        onView(withId(android.R.id.button1)).check(matches(withText("Upload"))).perform(click());
        helper.waitForCallback(crashListLoadInitCount, 1);
        // upload button is now hidden
        bodyDataInteraction
                .onChildView(withId(R.id.crash_upload_button))
                .check(matches(not(isDisplayed())));
        crashInfo.uploadState = UploadState.PENDING_USER_REQUESTED;
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        // Check that minidump file suffix is changed to ".forced"
        File renamedMinidumpFile =
                new File(minidumpFile.getAbsolutePath().replace("skipped", "forced"));
        assertThat("skipped minidump file shouldn't exist", not(minidumpFile.exists()));
        assertThat("renamed forced minidump file should exist", renamedMinidumpFile.exists());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testForceUploadSkippedCrashReport_withWifi() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456", systemTime, null, -1, FAKE_APP_PACKAGE_NAME, UploadState.SKIPPED);

        File minidumpFile = createMinidumpFile(crashInfo);
        assertThat("temp minidump file should exist", minidumpFile.exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CrashUploadUtil.setCrashUploadDelegateForTesting(
                new CrashUploadDelegate() {
                    @Override
                    public void scheduleNewJob(Context context, boolean requiresUnmeteredNetwork) {}

                    @Override
                    public boolean isNetworkUnmetered(Context context) {
                        return true;
                    }
                });

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // click on the crash item header to expand
        onData(anything()).atPosition(0).perform(click());
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        crashListLoadInitCount = helper.getCallCount();
        bodyDataInteraction.onChildView(withId(R.id.crash_upload_button)).perform(click());
        helper.waitForCallback(crashListLoadInitCount, 1);
        // upload button is now hidden
        bodyDataInteraction
                .onChildView(withId(R.id.crash_upload_button))
                .check(matches(not(isDisplayed())));
        crashInfo.uploadState = UploadState.PENDING_USER_REQUESTED;
        checkCrashItemUploadStatus(bodyDataInteraction, crashInfo);

        // Check that minidump file suffix is changed to ".forced"
        File renamedMinidumpFile =
                new File(minidumpFile.getAbsolutePath().replace("skipped", "forced"));
        assertThat("skipped minidump file shouldn't exist", not(minidumpFile.exists()));
        assertThat("renamed forced minidump file should exist", renamedMinidumpFile.exists());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    // Test when a crash has a known package name that can be found using PackageManager
    public void testInstalledPackageInfo() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
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
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456", systemTime, null, -1, appPackageName, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        DataInteraction headerDataInteraction = onData(anything()).atPosition(0);
        headerDataInteraction
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText(appPackageName)));
        headerDataInteraction
                .onChildView(withId(R.id.crash_package_icon))
                .check(matches(withDrawable(packageManager.getApplicationIcon(appInfo))));
    }

    @Test
    @Feature({"AndroidWebView"})
    // Test when app package name field is missing in the crash info.
    public void testMissingPackageInfo() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456", systemTime, null, -1, null, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        checkMissingPackageInfoCrashItemHeader(onData(anything()).atPosition(0), crashInfo);
    }

    @Test
    @Feature({"AndroidWebView"})
    // Test when crash is missing json, but has upload log file and minidump.
    public void testShowingSingleCrashReport_uploaded_missingJson() throws Throwable {
        CrashInfo crashInfo =
                createCrashInfoForTesting("123456", -1, null, 1000, null, UploadState.UPLOADED);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("upload log file should exist", appendUploadedEntryToLog(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        checkMissingPackageInfoCrashItemHeader(onData(anything()).atPosition(0), crashInfo);
    }

    @Test
    @Feature({"AndroidWebView"})
    // Test when crash is missing json, but has upload log file and minidump.
    public void testShowingSingleCrashReport_pending_missingJson() throws Throwable {
        CrashInfo crashInfo =
                createCrashInfoForTesting("123456", -1, null, 1000, null, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        checkMissingPackageInfoCrashItemHeader(onData(anything()).atPosition(0), crashInfo);
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
            crashInfo[i] =
                    createCrashInfoForTesting(
                            "abcd" + Integer.toString(i),
                            systemTime + i * 2000L,
                            null,
                            -1,
                            FAKE_APP_PACKAGE_NAME,
                            UploadState.PENDING);

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
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456",
                        systemTime,
                        "0abcde123456",
                        systemTime + 1000,
                        FAKE_APP_PACKAGE_NAME,
                        UploadState.UPLOADED);

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
        clickHideCrashButton(bodyDataInteraction);
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(0)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testHideCrashButton_pending() throws Throwable {
        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
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
        clickHideCrashButton(bodyDataInteraction);
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(0)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testHideCrashButton_uploaded_missingJson() throws Throwable {
        CrashInfo crashInfo =
                createCrashInfoForTesting("123456", -1, null, 1000, null, UploadState.UPLOADED);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("upload log file should exist", appendUploadedEntryToLog(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkMissingPackageInfoCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);

        crashListLoadInitCount = helper.getCallCount();
        clickHideCrashButton(bodyDataInteraction);
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(0)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testHideCrashButton_pending_missingJson() throws Throwable {
        CrashInfo crashInfo =
                createCrashInfoForTesting("123456", -1, null, -1, null, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));

        // Check crash item header
        checkMissingPackageInfoCrashItemHeader(onData(anything()).atPosition(0), crashInfo)
                .perform(click()); // click to expand it
        // The body is considered item#2 in the list view after expansion
        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));
        DataInteraction bodyDataInteraction = onData(anything()).atPosition(1);

        crashListLoadInitCount = helper.getCallCount();
        clickHideCrashButton(bodyDataInteraction);
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(0)));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testRefreshMenuOption() throws Throwable {
        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(0)));

        final long systemTime = System.currentTimeMillis();
        CrashInfo crashInfo =
                createCrashInfoForTesting(
                        "123456", systemTime, null, -1, FAKE_APP_PACKAGE_NAME, UploadState.PENDING);

        assertThat("temp minidump file should exist", createMinidumpFile(crashInfo).exists());
        assertThat("temp json log file should exist", writeJsonLogFile(crashInfo).exists());

        crashListLoadInitCount = helper.getCallCount();
        onView(withText("Refresh")).check(matches(isDisplayed()));
        onView(withText("Refresh")).perform(click());
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(1)));
        checkUnknownPackageCrashItemHeader(onData(anything()).atPosition(0), crashInfo);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.R,
            message = "https://crbug.com/1292197")
    public void testLongPressCopy() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        final long systemTime = System.currentTimeMillis();
        CrashInfo uploadedCrashInfo =
                createCrashInfoForTesting(
                        "123456",
                        systemTime - 1000,
                        "0abcde123456",
                        systemTime,
                        FAKE_APP_PACKAGE_NAME,
                        UploadState.UPLOADED);
        CrashInfo pendingCrashInfo =
                createCrashInfoForTesting(
                        "78910",
                        systemTime - 2000,
                        null,
                        -1,
                        FAKE_APP_PACKAGE_NAME,
                        UploadState.PENDING);

        assertThat(
                "temp json log file for uploaded crash should exist",
                writeJsonLogFile(uploadedCrashInfo).exists());
        assertThat(
                "upload log file should exist",
                appendUploadedEntryToLog(uploadedCrashInfo).exists());

        assertThat(
                "temp minidump file for pending crash should exist",
                createMinidumpFile(pendingCrashInfo).exists());
        assertThat(
                "temp json log file for pending crash should exist",
                writeJsonLogFile(pendingCrashInfo).exists());

        CallbackHelper helper = getCrashListLoadedListener();
        int crashListLoadInitCount = helper.getCallCount();
        launchCrashesFragment();
        helper.waitForCallback(crashListLoadInitCount, 1);

        onView(withId(R.id.crashes_list)).check(matches(withCount(2)));

        // click on the first crash item header to expand
        onData(anything()).atPosition(0).perform(click());
        // long click on the crash item body to copy
        onData(anything()).atPosition(1).perform(longClick());
        String expectedUploadInfo =
                new Date(uploadedCrashInfo.uploadTime).toString()
                        + "\nID: "
                        + uploadedCrashInfo.uploadId;
        assertThat(getClipBoardTextOnUiThread(context), is(expectedUploadInfo));

        // click on the first crash item header to collapse
        onData(anything()).atPosition(0).perform(click());
        // click on the second crash item header to expand
        onData(anything()).atPosition(1).perform(click());
        // Clear clipboard content
        setClipBoardTextOnUiThread(context, "", "");
        // Crash body is now the second item in the list view, long click on the crash item body to
        // copy.
        onData(anything()).atPosition(2).perform(longClick());
        // This a pending upload, nothing should be copied
        assertThat(getClipBoardTextOnUiThread(context), is(""));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConsentErrorMessage_notShown_differentWebViewPackageIsShown() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        // Inject a fake PackageInfo as the current WebView package to make sure it will always be
        // different from the test's app package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                HomeFragmentTest.FAKE_WEBVIEW_PACKAGE);
        PlatformServiceBridge.injectInstance(
                new TestPlatformServiceBridge(/* canUseGms= */ true, /* userConsent= */ false));
        launchCrashesFragment();

        String expectedErrorMessage =
                String.format(
                        Locale.US,
                        WebViewPackageError.DIFFERENT_WEBVIEW_PROVIDER_ERROR_MESSAGE,
                        WebViewPackageHelper.loadLabel(context));
        onView(withId(R.id.main_error_view)).check(matches(isDisplayed()));
        onView(withId(R.id.error_text)).check(matches(withText(expectedErrorMessage)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConsentErrorMessage_notShown_userConsented() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        // Inject test app package as the current WebView package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        PlatformServiceBridge.injectInstance(
                new TestPlatformServiceBridge(/* canUseGms= */ true, /* userConsent= */ true));
        launchCrashesFragment();

        onView(withId(R.id.main_error_view)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConsentErrorMessage_shown_canUseGms() throws Throwable {
        Context context = ContextUtils.getApplicationContext();

        Intent settingsIntent =
                new Intent(CrashesListFragment.USAGE_AND_DIAGONSTICS_ACTIVITY_INTENT_ACTION);
        List<ResolveInfo> intentResolveInfo =
                context.getPackageManager().queryIntentActivities(settingsIntent, 0);
        Assume.assumeTrue(
                "This test assumes \"usage& diagonstics\" settings can be found on the device",
                intentResolveInfo.size() > 0);

        // Inject test app package as the current WebView package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        PlatformServiceBridge.injectInstance(
                new TestPlatformServiceBridge(/* canUseGms= */ true, /* userConsent= */ false));
        launchCrashesFragment();

        onView(withId(R.id.main_error_view)).check(matches(isDisplayed()));
        onView(withId(R.id.error_text))
                .check(
                        matches(
                                withText(
                                        CrashesListFragment
                                                .CRASH_COLLECTION_DISABLED_ERROR_MESSAGE)));
        onView(withId(R.id.action_button))
                .check(matches(withText("Open Settings")))
                .perform(click());
        intended(
                IntentMatchers.hasAction(
                        CrashesListFragment.USAGE_AND_DIAGONSTICS_ACTIVITY_INTENT_ACTION));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConsentErrorMessage_shown_onlyInCrashFragment() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        // Inject test app package as the current WebView package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        PlatformServiceBridge.injectInstance(
                new TestPlatformServiceBridge(/* canUseGms= */ true, /* userConsent= */ false));
        launchCrashesFragment();

        onView(withId(R.id.main_error_view)).check(matches(isDisplayed()));
        onView(withId(R.id.error_text))
                .check(
                        matches(
                                withText(
                                        CrashesListFragment
                                                .CRASH_COLLECTION_DISABLED_ERROR_MESSAGE)));

        // CrashesListFragment -> FlagsFragment (Not shown)
        onView(withId(R.id.navigation_flags_ui)).perform(click());
        onView(withId(R.id.fragment_flags)).check(matches(isDisplayed()));
        onView(withId(R.id.main_error_view)).check(matches(not(isDisplayed())));
        // FlagsFragment -> HomeFragment (Not shown)
        onView(withId(R.id.navigation_home)).perform(click());
        onView(withId(R.id.fragment_home)).check(matches(isDisplayed()));
        onView(withId(R.id.main_error_view)).check(matches(not(isDisplayed())));
        // HomeFragment -> CrashesListFragment (shown again)
        onView(withId(R.id.navigation_crash_ui)).perform(click());
        onView(withId(R.id.fragment_crashes_list)).check(matches(isDisplayed()));
        onView(withId(R.id.main_error_view)).check(matches(isDisplayed()));
        onView(withId(R.id.error_text))
                .check(
                        matches(
                                withText(
                                        CrashesListFragment
                                                .CRASH_COLLECTION_DISABLED_ERROR_MESSAGE)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConsentErrorMessage_shown_cannotUseGms() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        // Inject test app package as the current WebView package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        PlatformServiceBridge.injectInstance(
                new TestPlatformServiceBridge(/* canUseGms= */ false, /* userConsent= */ false));
        launchCrashesFragment();

        onView(withId(R.id.main_error_view)).check(matches(isDisplayed()));
        onView(withId(R.id.error_text))
                .check(matches(withText(CrashesListFragment.NO_GMS_ERROR_MESSAGE)));
        onView(withId(R.id.action_button)).check(matches(not(isDisplayed())));
    }
}
