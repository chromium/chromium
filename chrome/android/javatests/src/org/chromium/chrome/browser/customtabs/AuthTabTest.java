// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.content.Intent;

import androidx.browser.auth.AuthTabIntent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for Auth Tab. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Testing CCT start up behavior.")
@Features.EnableFeatures(ChromeFeatureList.CCT_AUTH_TAB)
public class AuthTabTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/auth.html";
    private static final String SCHEME = "testscheme";
    private static final String JS_CLICK_AUTH_BUTTON =
            "document.getElementById('authButton').click();";

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private Context mApplicationContext;
    private String mTestPage;
    private EmbeddedTestServer mTestServer;
    private AuthTabIntent.AuthResult mLastAuthResult;
    private CallbackHelper mAuthResultCallbackHelper = new CallbackHelper();

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        mApplicationContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(mApplicationContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void testCustomSchemeSuccess() throws TimeoutException {
        launchAuthTab();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mCustomTabActivityTestRule.getActivity().getCurrentWebContents(),
                JS_CLICK_AUTH_BUTTON);
        mAuthResultCallbackHelper.waitForNext();
        assertEquals(AuthTabIntent.RESULT_OK, mLastAuthResult.resultCode);
        assertEquals("sometoken", mLastAuthResult.resultUri.getQueryParameter("token"));
    }

    @Test
    @MediumTest
    public void testCustomSchemeCanceled() throws TimeoutException {
        launchAuthTab();
        ClickUtils.clickButton(
                mCustomTabActivityTestRule
                        .getActivity()
                        .findViewById(org.chromium.chrome.test.R.id.close_button));
        mAuthResultCallbackHelper.waitForNext();
        assertEquals(AuthTabIntent.RESULT_CANCELED, mLastAuthResult.resultCode);
        assertNull(mLastAuthResult.resultUri);
    }

    private void launchAuthTab() {
        BlankAuthTabLauncherTestActivity launcherActivity = startLauncherActivity();
        Callback<AuthTabIntent.AuthResult> authCallback =
                result -> {
                    mLastAuthResult = result;
                    mAuthResultCallbackHelper.notifyCalled();
                };
        CustomTabActivity cctActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () ->
                                launcherActivity.launchAuthTab(
                                        mApplicationContext.getPackageName(),
                                        mTestPage,
                                        SCHEME,
                                        authCallback));
        mCustomTabActivityTestRule.setActivity(cctActivity);
        mCustomTabActivityTestRule.waitForActivityCompletelyLoaded();
    }

    private BlankAuthTabLauncherTestActivity startLauncherActivity() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent emptyIntent = new Intent(context, BlankAuthTabLauncherTestActivity.class);
        emptyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return (BlankAuthTabLauncherTestActivity)
                InstrumentationRegistry.getInstrumentation().startActivitySync(emptyIntent);
    }
}
