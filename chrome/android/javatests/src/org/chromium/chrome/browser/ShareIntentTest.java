// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Instrumentation tests for Share intents.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareIntentTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @LargeTest
    public void testDirectShareIntent() throws Exception {
        ComponentName target = new ComponentName("test.package", "test.activity");
        ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(target.getClassName(),
                        new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ShareHelper.setLastShareComponentName(Profile.getLastUsedRegularProfile(), target);
            mActivityTestRule.getActivity().onMenuOrKeyboardAction(R.id.direct_share_menu_id, true);
        });
        CriteriaHelper.pollUiThread(
                () -> { Criteria.checkThat(monitor.getHits(), Matchers.is(1)); });
    }
}
