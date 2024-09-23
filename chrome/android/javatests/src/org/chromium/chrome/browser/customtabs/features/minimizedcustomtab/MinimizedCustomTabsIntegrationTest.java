// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.createMinimalCustomTabIntent;
import static org.chromium.ui.test.util.DeviceRestriction.RESTRICTION_TYPE_NON_AUTO;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.Intent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.net.test.EmbeddedTestServer;

/** Integration tests for the Minimized Custom Tabs feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "Activity needs to restart after tests because we can't exit PiP programmatically.")
@Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE, RESTRICTION_TYPE_NON_AUTO})
@EnableFeatures({ChromeFeatureList.CCT_MINIMIZED})
public class MinimizedCustomTabsIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "b/325487558")
    public void testMinimize() {
        var intent = createIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        onViewWaiting(withId(R.id.custom_tabs_minimize_button)).perform(click());

        CriteriaHelper.pollUiThread(
                () -> mCustomTabActivityTestRule.getActivity().isInPictureInPictureMode());

        assertEquals(
                View.VISIBLE,
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.card).getVisibility());
    }

    private Intent createIntent() {
        return createMinimalCustomTabIntent(ApplicationProvider.getApplicationContext(), mTestPage);
    }
}
