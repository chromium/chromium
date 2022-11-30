// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasHost;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasParamWithValue;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasPath;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasScheme;

import static org.hamcrest.MatcherAssert.assertThat;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.content.Intent;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.devui.util.CrashBugUrlFactory;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for CrashBugUrlFactory class
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class CrashBugUrlFactoryTest {
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetReportIntent() throws Throwable {
        Map<String, String> crashKeys = new HashMap<>();
        crashKeys.put(CrashInfo.APP_PACKAGE_NAME_KEY, "org.test.package");
        crashKeys.put(CrashInfo.APP_PACKAGE_VERSION_CODE_KEY, "1.0.2.3");
        crashKeys.put(CrashInfo.WEBVIEW_VERSION_KEY, "10.0.1234.5");
        crashKeys.put(CrashInfo.ANDROID_SDK_INT_KEY, "100");
        CrashInfo crashInfo = new CrashInfo("12345678", crashKeys);
        crashInfo.uploadId = "a1b2c3d4";

        final String expectedDescription = ""
                + "Build fingerprint: " + Build.FINGERPRINT + "\n"
                + "Android API level: 100\n"
                + "Crashed WebView version: 10.0.1234.5\n"
                + "DevTools version: " + CrashBugUrlFactory.getCurrentDevToolsVersion() + "\n"
                + "Application: org.test.package (1.0.2.3)\n"
                + "If this app is available on Google Play, please include a URL:\n"
                + "\n"
                + "\n"
                + "Steps to reproduce:\n"
                + "(1)\n"
                + "(2)\n"
                + "(3)\n"
                + "\n"
                + "\n"
                + "Expected result:\n"
                + "(What should have happened?)\n"
                + "\n"
                + "\n"
                + "<Any additional comments, you want to share>"
                + "\n"
                + "\n"
                + "****DO NOT CHANGE BELOW THIS LINE****\n"
                + "Crash ID: http://crash/a1b2c3d4\n"
                + "Instructions for triaging this report (Chromium members only): "
                + "https://bit.ly/2SM1Y9t\n";

        Intent intent = new CrashBugUrlFactory(crashInfo).getReportIntent();
        assertThat(intent, hasAction(Intent.ACTION_VIEW));
        assertThat(intent, hasData(hasScheme("https")));
        assertThat(intent, hasData(hasHost("bugs.chromium.org")));
        assertThat(intent, hasData(hasPath("/p/chromium/issues/entry")));
        assertThat(intent, hasData(hasParamWithValue("template", "Webview+Bugs")));
        assertThat(intent,
                hasData(hasParamWithValue("labels",
                        "User-Submitted,Via-WebView-DevTools,Pri-3,Type-Bug,OS-Android"
                                + ",FoundIn-10")));
        assertThat(intent, hasData(hasParamWithValue("description", expectedDescription)));
    }
}