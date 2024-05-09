// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.anything;

import android.content.Context;
import android.content.Intent;
import android.widget.ListView;

import androidx.test.espresso.DataInteraction;
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
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
        TestThreadUtils.runOnUiThreadBlocking(
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
                            + MOCK_PACKAGE_NAME
                            + "_"
                            + package_num
                            + JSON_TAG;
            File file =
                    new File(
                            ContextUtils.getApplicationContext().getFilesDir().getPath(), fileName);
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
    public void testCorrectNumberOfFiles() throws Throwable {
        ListView filesList = mRule.getActivity().findViewById(R.id.net_log_list);
        int totalNumFiles = filesList.getCount();
        Assert.assertEquals(5, totalNumFiles);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCorrectListDisplay() throws Throwable {
        ListView filesList = mRule.getActivity().findViewById(R.id.net_log_list);
        int totalNumFiles = filesList.getCount();
        Assert.assertEquals(5, totalNumFiles);

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
}
