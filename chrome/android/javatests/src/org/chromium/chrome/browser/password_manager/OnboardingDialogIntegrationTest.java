// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static android.support.test.espresso.Espresso.pressBack;

import static org.chromium.chrome.browser.password_manager.OnboardingState.SHOULD_SHOW;
import static org.chromium.chrome.browser.preferences.Pref.PASSWORD_MANAGER_ONBOARDING_STATE;
import static org.chromium.net.test.ServerCertificate.CERT_OK;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.support.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.IntegrationTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for the password manager onboarding flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.PASSWORD_MANAGER_ONBOARDING_ANDROID})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OnboardingDialogIntegrationTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String SUBMIT_NODE_ID = "input_submit_button";
    private static final String FORM_URL =
            "/chrome/test/data/password/filled_simple_password_form.html";

    private EmbeddedTestServer mEmbeddedTestServer;
    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<>();

    public void loadTestPage() {
        mEmbeddedTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(), CERT_OK);
        mSyncTestRule.startMainActivityWithURL(mEmbeddedTestServer.getURL(FORM_URL));
        setRtlForTesting(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeActivity activity = mSyncTestRule.getActivity();
            mWebContentsRef.set(activity.getActivityTab().getWebContents());
        });
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), SUBMIT_NODE_ID);
    }

    @Before
    public void setUp() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setInteger(
                    PASSWORD_MANAGER_ONBOARDING_STATE, SHOULD_SHOW);
        });
        loadTestPage();
    }

    @After
    public void tearDown() {
        if (mEmbeddedTestServer != null) mEmbeddedTestServer.stopAndDestroyServer();
    }

    @Test
    @IntegrationTest
    public void testOnboardingIsShown() throws TimeoutException {
        DOMUtils.clickNode(mWebContentsRef.get(), SUBMIT_NODE_ID);
        waitForView(R.id.password_manager_dialog);
    }

    @Test
    @IntegrationTest
    public void testOnboardingAccepted() throws TimeoutException {
        DOMUtils.clickNode(mWebContentsRef.get(), SUBMIT_NODE_ID);
        waitForView(R.id.password_manager_dialog);
        clickOnView(R.id.positive_button);
        CriteriaHelper.pollUiThread(() -> {
            return InfoBarContainer.from(mSyncTestRule.getActivity().getActivityTab())
                    .hasInfoBars();
        });
    }

    @Test
    @IntegrationTest
    public void testOnboardingRejected() throws TimeoutException {
        DOMUtils.clickNode(mWebContentsRef.get(), SUBMIT_NODE_ID);
        waitForView(R.id.password_manager_dialog);
        clickOnView(R.id.negative_button);
        assertNoInfobarsAreShown();
    }

    @Test
    @IntegrationTest
    public void testOnboardingDismissedPressedBack() throws TimeoutException {
        DOMUtils.clickNode(mWebContentsRef.get(), SUBMIT_NODE_ID);
        waitForView(R.id.password_manager_dialog);
        pressBack();
        assertNoInfobarsAreShown();
    }

    private void assertNoInfobarsAreShown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(InfoBarContainer.from(mSyncTestRule.getActivity().getActivityTab())
                                       .hasInfoBars());
        });
    }

    private void clickOnView(int id) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSyncTestRule.getActivity().findViewById(id).performClick(); });
    }

    private void waitForView(int id) {
        CriteriaHelper.pollUiThread(() -> mSyncTestRule.getActivity().findViewById(id) != null);
    }
}
