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

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Intent;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.BugTrackerConstants;
import org.chromium.android_webview.devui.util.CrashBugUrlFactory;
import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/** Unit tests for CrashBugUrlFactory class */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
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

        final String expectedDescription =
                String.format(
                        Locale.US,
                        CrashBugUrlFactory.CRASH_REPORT_TEMPLATE,
                        Build.FINGERPRINT,
                        /* CrashInfo.ANDROID_SDK_INT_KEY */ "100",
                        /* CrashInfo.WEBVIEW_VERSION_KEY */ "10.0.1234.5",
                        CrashBugUrlFactory.getCurrentDevToolsVersion(),
                        /* CrashAppPackageInfo */ "org.test.package (1.0.2.3)",
                        crashInfo.uploadId);

        Intent intent = new CrashBugUrlFactory(crashInfo).getReportIntent();
        assertThat(intent, hasAction(Intent.ACTION_VIEW));
        assertThat(intent, hasData(hasScheme("https")));
        assertThat(intent, hasData(hasHost("issues.chromium.org")));
        assertThat(intent, hasData(hasPath("/issues/new")));
        assertThat(
                intent,
                hasData(
                        hasParamWithValue(
                                "component", BugTrackerConstants.COMPONENT_MOBILE_WEBVIEW)));
        assertThat(
                intent,
                hasData(
                        hasParamWithValue(
                                "template", BugTrackerConstants.DEFAULT_WEBVIEW_TEMPLATE)));
        assertThat(intent, hasData(hasParamWithValue("priority", "P3")));
        assertThat(intent, hasData(hasParamWithValue("type", "BUG")));
        assertThat(
                intent,
                hasData(
                        hasParamWithValue(
                                "customFields", BugTrackerConstants.OS_FIELD + ":Android")));
        assertThat(
                intent,
                hasData(
                        hasParamWithValue(
                                "hotlistIds", BugTrackerConstants.USER_SUBMITTED_HOTLIST)));
        assertThat(intent, hasData(hasParamWithValue("foundIn", "10")));
        assertThat(intent, hasData(hasParamWithValue("description", expectedDescription)));
    }
}
