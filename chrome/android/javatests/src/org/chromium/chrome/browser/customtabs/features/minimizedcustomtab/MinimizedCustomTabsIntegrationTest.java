// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.createMinimalCustomTabIntent;
import static org.chromium.ui.test.util.DeviceRestriction.RESTRICTION_TYPE_NON_AUTO;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.text.Layout;
import android.text.TextUtils.TruncateAt;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

/** Integration tests for the Minimized Custom Tabs feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
})
@DoNotBatch(
        reason =
                "Activity needs to restart after tests because we can't exit PiP programmatically.")
@Restriction(RESTRICTION_TYPE_NON_AUTO)
public class MinimizedCustomTabsIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String RTL_SPOOF_HOST = "xn--mgb.google.com.xn--mgb.yen.comma.sbs";

    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);
        // Desktop Android launches activities in freeform multi-window mode, which hides the
        // toolbar minimize button; force single-window evaluation so the button is testable on
        // both phone and desktop form factors.
        MultiWindowUtils.setInstanceForTesting(
                new MultiWindowUtils() {
                    @Override
                    public boolean isInMultiWindowMode(Activity activity) {
                        return false;
                    }
                });
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
    public void testMinimize() {
        var intent = createIntent(mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        minimizeAndWaitForCard();
    }

    @Test
    @MediumTest
    public void testMinimize_rtlIdnDisplaysPunycodeAndLtr() {
        String rtlUrl = mTestServer.getURLWithHostName(RTL_SPOOF_HOST, TEST_PAGE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createIntent(rtlUrl));

        minimizeAndWaitForCard();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String expected =
                            UrlFormatter.formatUrlForSecurityDisplay(
                                    new GURL(rtlUrl), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
                    TextView urlView =
                            mCustomTabActivityTestRule.getActivity().findViewById(R.id.url);
                    String shown = urlView.getText().toString();
                    assertEquals(expected, shown);
                    assertFalse(shown.contains("\u0627"));
                    assertEquals(View.TEXT_DIRECTION_LTR, urlView.getTextDirection());
                    assertEquals(TruncateAt.START, urlView.getEllipsize());
                    assertNotNull(urlView.getLayout());
                    assertEquals(
                            Layout.DIR_LEFT_TO_RIGHT, urlView.getLayout().getParagraphDirection(0));
                });
    }

    private void minimizeAndWaitForCard() {
        waitForView(withId(R.id.custom_tabs_minimize_button)).click();

        CriteriaHelper.pollUiThread(
                () -> {
                    Activity activity = mCustomTabActivityTestRule.getActivity();
                    TextView urlView = activity.findViewById(R.id.url);
                    return activity.isInPictureInPictureMode()
                            && activity.findViewById(R.id.card) != null
                            && urlView != null
                            && urlView.getLayout() != null;
                });

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                View.VISIBLE,
                                mCustomTabActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.card)
                                        .getVisibility()));
    }

    private Intent createIntent(String url) {
        return createMinimalCustomTabIntent(ApplicationProvider.getApplicationContext(), url);
    }
}
