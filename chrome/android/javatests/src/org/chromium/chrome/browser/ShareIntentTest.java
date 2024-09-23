// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Intent;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;

/** Instrumentation tests for Share intents. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ShareIntentTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @LargeTest
    public void testDirectShareIntent() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        ComponentName target = new ComponentName("test.package", "test.activity");
        ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                target.getClassName(),
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShareHelper.setLastShareComponentName(
                            ProfileManager.getLastUsedRegularProfile(), target);
                    mActivityTestRule
                            .getActivity()
                            .onMenuOrKeyboardAction(R.id.direct_share_menu_id, true);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(monitor.getHits(), Matchers.is(1));
                });
    }

    @Test
    @LargeTest
    public void testReceiveShareIntent() throws Exception {
        String url = mActivityTestRule.getTestServer().getURL("/content/test/data/hello.html");
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TEXT, "This is a share:\n" + url);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        mActivityTestRule.startActivityCompletely(intent);
        ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivity().getActivityTab(), url);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.SHARE_INTENT));
    }
}
