// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.greaterThan;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.Intent;
import android.widget.ListView;

import androidx.test.espresso.DataInteraction;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.NetLogsFragment;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.services.AwNetLogService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.test.util.ViewUtils;

import java.io.File;
import java.io.IOException;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * UI tests for {@link NetLogsFragment}.
 *
 * <p>These tests should not be batched to make sure that the DeveloperUiService is killed after
 * each test, leaving a clean state.
 */
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Clean up DeveloperUiService after each test")
public class NetLogsFragmentTest {
    @Rule
    public BaseActivityTestRule<MainActivity> mRule =
            new BaseActivityTestRule<>(MainActivity.class);

    private static final String TAG = "NetLogsFragmentTest";
    private static final String JSON_TAG = ".json";
    private static final String MOCK_PID = "1234_";
    private static final String MOCK_PACKAGE_NAME = "package.name";
    private static List<File> sMockFileList;
    private long mFileTime;

    @Before
    public void setUp() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, MainActivity.class);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sMockFileList = initalizeTestFiles();
                    NetLogsFragment.setFileListForTesting(sMockFileList);
                });
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_NETLOGS);
        mRule.launchActivity(intent);
        waitForInflatedNetLogFragment();
    }

    @After
    public void tearDown() {
        ArrayList<File> filesToDelete = new ArrayList<>(sMockFileList);
        for (File file : filesToDelete) {
            file.delete();
        }
    }

    private List<File> initalizeTestFiles() {
        String package_num = "";
        mFileTime = System.currentTimeMillis();
        List<File> files = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            package_num += 'I';
            String fileName =
                    MOCK_PID
                            + Long.toString(mFileTime + i)
                            + "_"
                            + MOCK_PACKAGE_NAME
                            + package_num
                            + JSON_TAG;
            File file = new File(AwNetLogService.getNetLogFileDirectory(), fileName);
            try {
                file.createNewFile();
                files.add(file);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        return files;
    }

    private void waitForInflatedNetLogFragment() {
        // Espresso is normally configured to automatically wait for the main thread to go idle, but
        // BaseActivityTestRule turns that behavior off so we must explicitly wait for the View
        // hierarchy to inflate.
        ViewUtils.waitForVisibleView(withId(R.id.navigation_home));
        ViewUtils.waitForVisibleView(withId(R.id.net_log_list));
        ViewUtils.waitForVisibleView(withId(R.id.net_logs_total_capacity));
        ViewUtils.waitForVisibleView(withId(R.id.delete_all_net_logs_button));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHasPublicNoArgsConstructor() throws Throwable {
        NetLogsFragment fragment = new NetLogsFragment();
        Assert.assertNotNull(fragment);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCorrectListDisplay() throws Throwable {
        ListView filesList = mRule.getActivity().findViewById(R.id.net_log_list);
        Assert.assertEquals(5, filesList.getCount());

        String package_num = "";
        DateFormat dateFormat = DateFormat.getDateTimeInstance();

        for (int i = 0; i < filesList.getCount(); i++) {
            package_num += 'I';
            DataInteraction fileInteraction =
                    onData(anything()).inAdapterView(withId(R.id.net_log_list)).atPosition(i);

            fileInteraction
                    .onChildView(withId(R.id.file_name))
                    .check(matches(withText(MOCK_PACKAGE_NAME + package_num)));

            fileInteraction
                    .onChildView(withId(R.id.file_capacity))
                    .check(matches(withText("0.00 MB")));

            String date = dateFormat.format(new Date(mFileTime + i));

            fileInteraction.onChildView(withId(R.id.file_time)).check(matches(withText(date)));
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testDeleteAllNetLogsButton() throws Throwable {
        onView(withId(R.id.delete_all_net_logs_button)).perform(click());

        ListView filesList = mRule.getActivity().findViewById(R.id.net_log_list);
        Assert.assertEquals(0, filesList.getCount());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSortedFiles() throws Throwable {
        ListView filesList = mRule.getActivity().findViewById(R.id.net_log_list);

        File prevFile = (File) filesList.getAdapter().getItem(0);
        long prevTime = AwNetLogService.getCreationTimeFromFileName(prevFile.getName());

        for (int i = 1; i < filesList.getCount(); i++) {
            File currFile = (File) filesList.getAdapter().getItem(i);
            long currTime = AwNetLogService.getCreationTimeFromFileName(currFile.getName());
            assertThat(currTime, greaterThan(prevTime));

            prevTime = currTime;
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuDelete() throws Throwable {
        ListView filesList = mRule.getActivity().findViewById(R.id.net_log_list);
        Assert.assertEquals(5, filesList.getCount());

        File firstFile = (File) filesList.getAdapter().getItem(1);
        String firstFileName = NetLogsFragment.getFilePackageName(firstFile);
        onView(withText(firstFileName)).perform(click());
        onView(withText("Delete")).check(matches(isDisplayed()));
        onView(withText("Delete")).perform(click());

        Assert.assertEquals(4, filesList.getCount());

        for (int i = 0; i < filesList.getCount(); i++) {
            File file = (File) filesList.getAdapter().getItem(i);

            // Ensuring that the remaining files are not the first file that was deleted.
            Assert.assertNotEquals(NetLogsFragment.getFilePackageName(file), firstFileName);
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuShare() throws Throwable {
        try {
            Intents.init();

            // Stub out the Intent to the File Provider, to verify the case where the File Provider
            // Intent
            // resolves.
            intending(
                            allOf(
                                    IntentMatchers.hasAction(Intent.ACTION_CHOOSER),
                                    IntentMatchers.hasExtra(
                                            Intent.EXTRA_INTENT,
                                            allOf(
                                                    IntentMatchers.hasAction(Intent.ACTION_SEND),
                                                    IntentMatchers.hasType("application/json")))))
                    .respondWith(new ActivityResult(Activity.RESULT_OK, null));

            String firstFileName = NetLogsFragment.getFilePackageName(sMockFileList.get(1));
            onView(withText(firstFileName)).perform(click());
            onView(withText("Share")).check(matches(isDisplayed()));
            onView(withText("Share")).perform(click());

            intended(
                    allOf(
                            IntentMatchers.hasAction(Intent.ACTION_CHOOSER),
                            IntentMatchers.hasExtra(
                                    Intent.EXTRA_INTENT,
                                    allOf(
                                            IntentMatchers.hasAction(Intent.ACTION_SEND),
                                            IntentMatchers.hasType("application/json")))));
        } finally {
            Intents.release();
        }
    }
}
