// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.util.Pair;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.TestContentProvider;
import org.chromium.chrome.test.util.ActivityTestUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

/** Integration test suite for the MediaLauncherActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MediaLauncherActivityTest {
    @Before
    public void setUp() {
        MediaViewerUtils.forceEnableMediaLauncherActivityForTest();
    }

    @After
    public void tearDown() {
        ActivityFinisher.finishAll();
    }

    @Test
    @SmallTest
    public void testHandleVideoIntent() throws Exception {
        String url = TestContentProvider.createContentUrl("media/test.mp4");
        expectMediaToBeHandled(url, "video/mp4");
    }

    @Test
    @SmallTest
    public void testHandleAudioIntent() throws Exception {
        String url = TestContentProvider.createContentUrl("media/audio.mp3");
        expectMediaToBeHandled(url, "audio/mp3");
    }

    @Test
    @SmallTest
    public void testHandleImageIntent() throws Exception {
        String url = TestContentProvider.createContentUrl("google.png");
        expectMediaToBeHandled(url, "image/png");
    }

    @Test
    @SmallTest
    @MaxAndroidSdkLevel(
            value = Build.VERSION_CODES.S_V2,
            reason = "File access was locked down in T")
    public void testHandleFileURIIntent() throws Exception {
        String url = UrlUtils.getTestFileUrl("google.png");
        expectMediaToBeHandled(url, "image/png");
    }

    @Test
    @SmallTest
    public void testFilterURI() {
        List<Pair<String, String>> testCases =
                Arrays.asList(
                        new Pair<>("file:///test.jpg", "file:///test.jpg"),
                        new Pair<>("file:///test.jp!g", "file:///test.jp!g"),
                        new Pair<>("file:///test!$'.jpg", "file:///test.jpg"),
                        new Pair<>("file:///test!$'.jpg?x=y", "file:///test.jpg?x=y"),
                        new Pair<>(
                                "file:///test!$'.jpg?x=y#fragment!",
                                "file:///test.jpg?x=y#fragment!"));

        for (Pair<String, String> testCase : testCases) {
            String testInput = testCase.first;
            String expected = testCase.second;
            Assert.assertEquals(expected, MediaLauncherActivity.filterURI(Uri.parse(testInput)));
        }
    }

    private void expectMediaToBeHandled(String url, String mimeType) throws Exception {
        Context context = ContextUtils.getApplicationContext();
        Uri uri = Uri.parse(url);
        ComponentName componentName = new ComponentName(context, MediaLauncherActivity.class);
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);
        intent.setDataAndType(uri, mimeType);
        intent.setComponent(componentName);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        waitForCustomTabActivityToStart(
                () -> {
                    context.startActivity(intent);
                    return null;
                },
                url);
    }

    private void waitForCustomTabActivityToStart(Callable<Void> trigger, String expectedUrl)
            throws Exception {
        CustomTabActivity cta =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        CustomTabActivity.class,
                        trigger);

        CriteriaHelper.pollUiThreadLongTimeout(
                "Waiting for Tab URL to be " + expectedUrl,
                () -> {
                    Tab tab = cta.getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    Criteria.checkThat(tab.getUrl().getSpec(), Matchers.is(expectedUrl));
                });
    }
}
