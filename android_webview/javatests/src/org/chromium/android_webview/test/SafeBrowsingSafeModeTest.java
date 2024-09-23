// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingSafeModeAction;
import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/** Tests for AwSafeBrowsingSafeModeAction. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class SafeBrowsingSafeModeTest extends AwParameterizedTest {
    private static final String WEB_UI_MALWARE_URL = "chrome://safe-browsing/match?type=malware";

    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private AwContents mAwContents;

    public SafeBrowsingSafeModeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        // Need to configure user opt-in, otherwise WebView won't perform Safe Browsing checks.
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(true);
    }

    @After
    public void tearDown() {
        SafeModeController.getInstance().unregisterActionsForTesting();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingDisabledForHardcodedMalwareUrl() throws Throwable {
        SafeModeController safeModeController = SafeModeController.getInstance();
        safeModeController.registerActions(
                new SafeModeAction[] {new AwSafeBrowsingSafeModeAction()});
        safeModeController.executeActions(Set.of(SafeModeActionIds.DISABLE_AW_SAFE_BROWSING));

        mContentsClient = new TestAwContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), WEB_UI_MALWARE_URL);
        // If we get here, it means the navigation was not blocked by an interstitial.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingDisabledOverridesPerWebViewToggle() throws Throwable {
        SafeModeController safeModeController = SafeModeController.getInstance();
        safeModeController.registerActions(
                new SafeModeAction[] {new AwSafeBrowsingSafeModeAction()});
        safeModeController.executeActions(Set.of(SafeModeActionIds.DISABLE_AW_SAFE_BROWSING));

        mContentsClient = new TestAwContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), WEB_UI_MALWARE_URL);
        // If we get here, it means the navigation was not blocked by an interstitial.
    }

    @Test
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeActionSavesState() throws Throwable {
        assertFalse(AwSafeBrowsingSafeModeAction.isSafeBrowsingDisabled());
        new AwSafeBrowsingSafeModeAction().execute();
        assertTrue(AwSafeBrowsingSafeModeAction.isSafeBrowsingDisabled());
    }

    @Test
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInitSafeBrowsingSkipsGMSCoreCommunication() throws Throwable {
        SafeModeController safeModeController = SafeModeController.getInstance();
        safeModeController.registerActions(
                new SafeModeAction[] {new AwSafeBrowsingSafeModeAction()});
        safeModeController.executeActions(Set.of(SafeModeActionIds.DISABLE_AW_SAFE_BROWSING));

        MockPlatformServiceBridge mockPlatformServiceBridge = new MockPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mockPlatformServiceBridge);

        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        AwContentsStatics.initSafeBrowsing(
                InstrumentationRegistry.getInstrumentation().getTargetContext(),
                b -> helper.notifyCalled());
        helper.waitForCallback(count);

        Assert.assertFalse(
                "Should not call warmUpSafeBrowsing as GMSCore Communication should be skipped",
                mockPlatformServiceBridge.wasWarmUpSafeBrowsingCalled());
    }

    private static class MockPlatformServiceBridge extends PlatformServiceBridge {
        private boolean mWarmUpSafeBrowsingCalled;

        @Override
        public void warmUpSafeBrowsing(Context context, @NonNull final Callback<Boolean> callback) {
            mWarmUpSafeBrowsingCalled = true;
            // The test doesn't depend on whether we invoke the callback with true or false.
            callback.onResult(true);
        }

        public boolean wasWarmUpSafeBrowsingCalled() {
            return mWarmUpSafeBrowsingCalled;
        }
    }
}
