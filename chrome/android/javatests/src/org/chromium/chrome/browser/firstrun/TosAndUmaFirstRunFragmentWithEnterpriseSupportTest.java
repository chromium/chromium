// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.CheckBox;

import androidx.annotation.IntDef;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.enterprise.util.FakeEnterpriseInfo;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.policy.PolicyService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Test for first run activity and {@link TosAndUmaFirstRunFragmentWithEnterpriseSupport}.
 * For the outside signals that used in this test so that the verification is focusing on the
 * workflow and UI transition.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TosAndUmaFirstRunFragmentWithEnterpriseSupportTest {
    @IntDef({FragmentState.LOADING, FragmentState.NO_POLICY, FragmentState.HAS_POLICY,
            FragmentState.WAITING_UNTIL_NEXT_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    @interface FragmentState {
        int LOADING = 0;
        int NO_POLICY = 1;
        int HAS_POLICY = 2;
        int WAITING_UNTIL_NEXT_PAGE = 3;
    }

    @IntDef({SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.FASTER,
            SpeedComparedToInflation.SLOWER})
    @Retention(RetentionPolicy.SOURCE)
    @interface SpeedComparedToInflation {
        int NOT_RECORDED = 0;
        int FASTER = 1;
        int SLOWER = 2;
    }

    private static final int RENDER_TEST_REVISION = 2;
    private static final String RENDER_TEST_REVISION_DESCRIPTION =
            "Change the button padding and font family.";

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_REVISION_DESCRIPTION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_FIRST_RUN)
                    .build();
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock
    public FirstRunAppRestrictionInfo mMockAppRestrictionInfo;
    @Mock
    public PolicyService mPolicyService;
    @Mock
    public FirstRunUtils.Natives mFirstRunUtils;
    @Mock
    private PrivacyPreferencesManagerImpl mPrivacyPreferencesManagerMock;
    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    @Spy
    public ChromeBrowserInitializer mInitializer;
    @Captor
    public ArgumentCaptor<BrowserParts> mBrowserParts;

    public FakeEnterpriseInfo mFakeEnterpriseInfo = new FakeEnterpriseInfo();
    private FirstRunActivity mActivity;
    private final List<PolicyService.Observer> mPolicyServiceObservers = new ArrayList<>();
    private final List<Callback<Boolean>> mAppRestrictionsCallbacks = new ArrayList<>();
    private final List<Callback<Long>> mAppRestrictionsDurationCallbacks = new ArrayList<>();
    private final CallbackHelper mAcceptTosCallbackHelper = new CallbackHelper();
    private CallbackHelper mOnNativeInitializedHelper = new CallbackHelper();
    private CallbackHelper mOnPolicyServiceInitializedHelper = new CallbackHelper();
    private CallbackHelper mOnHideLoadingUICompleteHelper = new CallbackHelper();
    private int mExitCount;

    private View mTosText;
    private View mAcceptButton;
    private View mLowerSpinner;
    private View mCenterSpinner;
    private View mPrivacyDisclaimer;
    private CheckBox mUmaCheckBox;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Assert.assertFalse(
                CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE));

        Mockito.doNothing()
                .when(mInitializer)
                .handlePostNativeStartup(anyBoolean(), any(BrowserParts.class));
        ChromeBrowserInitializer.setForTesting(mInitializer);

        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(mMockAppRestrictionInfo);
        ToSAndUMAFirstRunFragment.setShowUmaCheckBoxForTesting(true);
        PolicyServiceFactory.setPolicyServiceForTest(mPolicyService);
        FirstRunUtils.setDisableDelayOnExitFreForTest(true);
        FirstRunUtilsJni.TEST_HOOKS.setInstanceForTesting(mFirstRunUtils);

        EnterpriseInfo.setInstanceForTest(mFakeEnterpriseInfo);

        FREMobileIdentityConsistencyFieldTrial.setFirstRunTrialGroupForTesting(
                FREMobileIdentityConsistencyFieldTrial.DISABLED_GROUP);

        setAppRestrictionsMockNotInitialized();
        setPolicyServiceMockNotInitialized();

        // TODO(https://crbug.com/1113229): Rework this to not depend on {@link FirstRunActivity}
        // implementation details.
        mExitCount = 0;
        FirstRunActivity.setObserverForTest(new FirstRunActivity.FirstRunActivityObserver() {
            @Override
            public void onCreatePostNativeAndPoliciesPageSequence(FirstRunActivity caller) {}

            @Override
            public void onAcceptTermsOfService(FirstRunActivity caller) {
                mAcceptTosCallbackHelper.notifyCalled();
            }

            @Override
            public void onJumpToPage(FirstRunActivity caller, int position) {}

            @Override
            public void onUpdateCachedEngineName(FirstRunActivity caller) {}

            @Override
            public void onAbortFirstRunExperience(FirstRunActivity caller) {}

            @Override
            public void onExitFirstRun(FirstRunActivity caller) {
                mExitCount++;
            }
        });
        ToSAndUMAFirstRunFragment.setObserverForTesting(new ToSAndUMAFirstRunFragment.Observer() {
            @Override
            public void onNativeInitialized() {
                mOnNativeInitializedHelper.notifyCalled();
            }

            @Override
            public void onPolicyServiceInitialized() {
                mOnPolicyServiceInitializedHelper.notifyCalled();
            }

            @Override
            public void onHideLoadingUIComplete() {
                mOnHideLoadingUICompleteHelper.notifyCalled();
            }
        });
    }

    @After
    public void tearDown() {
        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(null);
        ToSAndUMAFirstRunFragment.setShowUmaCheckBoxForTesting(false);
        TosAndUmaFirstRunFragmentWithEnterpriseSupport.setOverrideOnExitFreRunnableForTest(null);
        PolicyServiceFactory.setPolicyServiceForTest(null);
        FirstRunUtils.setDisableDelayOnExitFreForTest(false);
        FirstRunUtilsJni.TEST_HOOKS.setInstanceForTesting(mFirstRunUtils);
        EnterpriseInfo.setInstanceForTest(null);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);
        if (mActivity != null) mActivity.finish();
    }

    @Test
    @SmallTest
    public void testNoRestriction() throws Exception {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.NOT_RECORDED);
        assertPolicyServiceInitDelayAfterNative(true, false);

        // Try to accept ToS.
        setMetricsReportDisabled();
        TestThreadUtils.runOnUiThreadBlocking((Runnable) mAcceptButton::performClick);
        Assert.assertTrue("Crash report should be enabled.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testNoRestriction_AcceptBeforeNative() throws Exception {
        launchFirstRunThroughCustomTabPreNative();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);

        // Try to accept ToS.
        setMetricsReportDisabled();
        TestThreadUtils.runOnUiThreadBlocking((Runnable) mAcceptButton::performClick);
        assertUIState(FragmentState.WAITING_UNTIL_NEXT_PAGE);
        Assert.assertFalse("Crash report should not be enabled before native initialized.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());

        // ToS should be accepted when native is initialized.
        startNativeInitializationAndWait();
        assertHistograms(true, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.NOT_RECORDED);
        assertPolicyServiceInitDelayAfterNative(false, false);
        String histogram = "MobileFre.TosFragment.SpinnerVisibleDuration";
        Assert.assertEquals(String.format("Histogram <%s> should be recorded.", histogram), 1,
                RecordHistogram.getHistogramTotalCountForTesting(histogram));
        Assert.assertTrue("Crash report should be enabled.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testNoRestriction_BeforeInflation() throws Exception {
        setAppRestrictionsMockInitialized(false);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(false, SpeedComparedToInflation.FASTER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.NOT_RECORDED);
        assertPolicyServiceInitDelayAfterNative(false, false);
    }

    @Test
    @SmallTest
    // TODO(https://crbug.com/1120859): Test the policy check when native initializes before
    // inflation. This will be possible when FragmentScenario is available.
    public void testDialogEnabled() throws Exception {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(true);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.SLOWER);
        assertPolicyServiceInitDelayAfterNative(true, true);

        // Try to accept ToS.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) mAcceptButton::performClick);
        Assert.assertTrue("Crash report should be enabled.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testDialogEnabled_BeforeAppRestrictions() throws Exception {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // When policy is loaded on fully managed device, we don't need app restriction.
        setPolicyServiceMockInitializedWithDialogEnabled(true);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.NOT_RECORDED,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.SLOWER);
        assertPolicyServiceInitDelayAfterNative(true, true);
    }

    @Test
    @SmallTest
    public void testNotOwnedDevice() throws Exception {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(false);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED);
        assertPolicyServiceInitDelayAfterNative(false, false);
    }

    @Test
    @SmallTest
    public void testNotOwnedDevice_AcceptBeforePolicy() throws Exception {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(false);
        assertUIState(FragmentState.NO_POLICY);

        // Try to accept Tos.
        setMetricsReportDisabled();
        TestThreadUtils.runOnUiThreadBlocking((Runnable) mAcceptButton::performClick);
        assertUIState(FragmentState.WAITING_UNTIL_NEXT_PAGE);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        CriteriaHelper.pollUiThread(()
                                            -> PrivacyPreferencesManagerImpl.getInstance()
                                                       .isUsageAndCrashReportingPermittedByUser());
        String histogram = "MobileFre.TosFragment.SpinnerVisibleDuration";
        Assert.assertEquals(String.format("Histogram <%s> should be recorded.", histogram), 1,
                RecordHistogram.getHistogramTotalCountForTesting(histogram));

        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        assertPolicyServiceInitDelayAfterNative(true, true);
    }

    @Test
    @SmallTest
    public void testNotOwnedDevice_InitBeforeInflation() throws Exception {
        setAppRestrictionsMockInitialized(true);
        setEnterpriseInfoInitializedWithDeviceOwner(false);

        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.NO_POLICY);

        // Despite EnterpriseInfo being ready to report owned state before inflation, there is no
        // gap on the UI thread between the request and inflation. So the posted response to the
        // EnterpriseInfo request is always slower.
        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED);
        assertPolicyServiceInitDelayAfterNative(false, false);
    }

    @Test
    @SmallTest
    public void testOwnedDevice_NoRestriction() throws Exception {
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED);
        assertPolicyServiceInitDelayAfterNative(true, false);
    }

    @Test
    @SmallTest
    public void testOwnedDevice_NoPolicy() throws Exception {
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(true);
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(true);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        assertPolicyServiceInitDelayAfterNative(true, true);
    }

    @Test
    @SmallTest
    public void testSkip_DeviceOwnedThenDialogPolicy() throws Exception {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(true);
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        assertPolicyServiceInitDelayAfterNative(true, true);
        Assert.assertFalse("Crash report should not be enabled.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testSkip_DialogPolicyThenDeviceOwned() throws Exception {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(true);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        assertPolicyServiceInitDelayAfterNative(true, true);
        Assert.assertFalse("Crash report should not be enabled.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testSkip_LateAppRestrictions() throws Exception {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        assertUIState(FragmentState.LOADING);

        // Skip should happen without app restrictions being completed.
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.NOT_RECORDED,
                SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.SLOWER);
        assertPolicyServiceInitDelayAfterNative(true, true);

        // assertUIState will verify that exit was not called a second time.
        setAppRestrictionsMockInitialized(true);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        Assert.assertFalse("Crash report should not be enabled.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testNullOwnedState() throws Exception {
        setAppRestrictionsMockInitialized(true);
        setPolicyServiceMockInitializedWithDialogEnabled(false);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // Null means loading checking if the device is owned failed. This should be treated the
        // same as not being owned, and no skipping should occur.
        mFakeEnterpriseInfo.initialize(null);
        assertUIState(FragmentState.NO_POLICY);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Flaky test - see: https://crbug.com/1171147")
    public void testAcceptTosWithoutCrashUpload() throws Exception {
        setAppRestrictionsMockInitialized(true);
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        setPolicyServiceMockInitializedWithDialogEnabled(true);

        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.NO_POLICY);

        // Accept ToS without check on UMA.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUmaCheckBox.setChecked(false);
            mAcceptButton.performClick();
        });

        mAcceptTosCallbackHelper.waitForCallback("Accept Tos is never called.", 0);
        Assert.assertFalse("Crash report should not be enabled.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "FirstRun"})
    public void testRender() throws Exception {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // Clear the focus on view to avoid unexpected highlight on background.
        // @TODO(https://crbug.com/c/1289293): Background sometimes are highlighted in render tests.
        View tosAndUmaFragment =
                mActivity.getSupportFragmentManager().getFragments().get(0).getView();
        Assert.assertNotNull(tosAndUmaFragment);
        TestThreadUtils.runOnUiThreadBlocking(tosAndUmaFragment::clearFocus);

        renderWithPortraitAndLandscape(tosAndUmaFragment, "fre_tosanduma_loading");

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);
        renderWithPortraitAndLandscape(tosAndUmaFragment, "fre_tosanduma_nopolicy");
    }

    /** Tests TosAndUmaFirstRunFragment with uma dialog */
    @Test
    @SmallTest
    @Feature({"RenderTest", "FirstRun"})
    @DisabledTest(message = "https://crbug.com/1370946")
    public void testRenderWithUmaDialog() throws Exception {
        FREMobileIdentityConsistencyFieldTrial.setFirstRunTrialGroupForTesting(
                FREMobileIdentityConsistencyFieldTrial.OLD_FRE_WITH_UMA_DIALOG_GROUP);
        launchFirstRunThroughCustomTab();
        setAppRestrictionsMockInitialized(false);
        // Clear the focus on view to avoid unexpected highlight on background.
        View tosAndUmaFragment =
                mActivity.getSupportFragmentManager().getFragments().get(0).getView();
        Assert.assertNotNull(tosAndUmaFragment);
        TestThreadUtils.runOnUiThreadBlocking(tosAndUmaFragment::clearFocus);

        Assert.assertEquals(
                "Uma checkbox should not be visible.", View.GONE, mUmaCheckBox.getVisibility());
        renderWithPortraitAndLandscape(tosAndUmaFragment, "fre_tosandumadialog_nopolicy");
    }

    /** Tests TosAndUmaFirstRunFragment with uma dialog */
    @Test
    @SmallTest
    @Feature({"RenderTest", "FirstRun"})
    public void testRenderWithUmaDialogForChildAccount() throws Exception {
        FREMobileIdentityConsistencyFieldTrial.setFirstRunTrialGroupForTesting(
                FREMobileIdentityConsistencyFieldTrial.OLD_FRE_WITH_UMA_DIALOG_GROUP);
        mAccountManagerTestRule.addAccount(
                AccountManagerTestRule.generateChildEmail("account@gmail.com"));
        launchFirstRunThroughCustomTab();
        setAppRestrictionsMockInitialized(false);
        // Clear the focus on view to avoid unexpected highlight on background.
        View tosAndUmaFragment =
                mActivity.getSupportFragmentManager().getFragments().get(0).getView();
        Assert.assertNotNull(tosAndUmaFragment);
        TestThreadUtils.runOnUiThreadBlocking(tosAndUmaFragment::clearFocus);

        Assert.assertEquals(
                "Uma checkbox should not be visible.", View.GONE, mUmaCheckBox.getVisibility());
        renderWithPortraitAndLandscape(
                tosAndUmaFragment, "fre_tosandumadialog_childaccount_nopolicy");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "FirstRun"})
    public void testRenderWithPolicy() throws Exception {
        final CallbackHelper onExitFreCallback = new CallbackHelper();
        TosAndUmaFirstRunFragmentWithEnterpriseSupport.setOverrideOnExitFreRunnableForTest(
                onExitFreCallback::notifyCalled);

        setAppRestrictionsMockInitialized(true);
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // Clear the focus on view to avoid unexpected highlight on background.
        View tosAndUmaFragment =
                mActivity.getSupportFragmentManager().getFragments().get(0).getView();
        Assert.assertNotNull(tosAndUmaFragment);
        TestThreadUtils.runOnUiThreadBlocking(tosAndUmaFragment::clearFocus);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        onExitFreCallback.waitForFirst("OnExitFreCallback is never invoked.");
        Assert.assertEquals("Privacy disclaimer is not visible", mPrivacyDisclaimer.getVisibility(),
                View.VISIBLE);

        renderWithPortraitAndLandscape(tosAndUmaFragment, "fre_tosanduma_withpolicy");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "FirstRun"})
    public void testRenderWhenMetricsReportingAreDisabled() throws Exception {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // Clear the focus on view to avoid unexpected highlight on background.
        View tosAndUmaFragment =
                mActivity.getSupportFragmentManager().getFragments().get(0).getView();
        Assert.assertNotNull(tosAndUmaFragment);
        TestThreadUtils.runOnUiThreadBlocking(tosAndUmaFragment::clearFocus);

        // Initialize policies.
        Mockito.when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);
        setPolicyServiceMockInitializedWithDialogEnabled(true);
        setAppRestrictionsMockInitialized(true);
        mOnPolicyServiceInitializedHelper.waitForCallback("policy service never initialized.", 0);
        mOnHideLoadingUICompleteHelper.waitForCallback("loading ui never hidden.", 0);

        renderWithPortraitAndLandscape(tosAndUmaFragment, "fre_metricsreportingdisabled");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "FirstRun"})
    public void testRenderWhenMetricsReportingAreDisabledWithUmaDialog() throws Exception {
        FREMobileIdentityConsistencyFieldTrial.setFirstRunTrialGroupForTesting(
                FREMobileIdentityConsistencyFieldTrial.OLD_FRE_WITH_UMA_DIALOG_GROUP);

        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // Clear the focus on view to avoid unexpected highlight on background.
        View tosAndUmaFragment =
                mActivity.getSupportFragmentManager().getFragments().get(0).getView();
        Assert.assertNotNull(tosAndUmaFragment);
        TestThreadUtils.runOnUiThreadBlocking(tosAndUmaFragment::clearFocus);

        // Initialize policies.
        Mockito.when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);
        setPolicyServiceMockInitializedWithDialogEnabled(true);
        setAppRestrictionsMockInitialized(true);
        mOnPolicyServiceInitializedHelper.waitForCallback("policy service never initialized.", 0);
        mOnHideLoadingUICompleteHelper.waitForCallback("loading ui never hidden.", 0);

        renderWithPortraitAndLandscape(tosAndUmaFragment, "fre_metricsreportingdisabled_umadialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "FirstRun"})
    public void testRenderWhenMetricsReportingAreDisabledWithUmaDialogForChildAccount()
            throws Exception {
        FREMobileIdentityConsistencyFieldTrial.setFirstRunTrialGroupForTesting(
                FREMobileIdentityConsistencyFieldTrial.OLD_FRE_WITH_UMA_DIALOG_GROUP);
        mAccountManagerTestRule.addAccount(
                AccountManagerTestRule.generateChildEmail("account@gmail.com"));

        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // Clear the focus on view to avoid unexpected highlight on background.
        View tosAndUmaFragment =
                mActivity.getSupportFragmentManager().getFragments().get(0).getView();
        Assert.assertNotNull(tosAndUmaFragment);
        TestThreadUtils.runOnUiThreadBlocking(tosAndUmaFragment::clearFocus);

        // Initialize policies.
        Mockito.when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);
        setPolicyServiceMockInitializedWithDialogEnabled(true);
        setAppRestrictionsMockInitialized(true);
        mOnPolicyServiceInitializedHelper.waitForCallback("policy service never initialized.", 0);
        mOnHideLoadingUICompleteHelper.waitForCallback("loading ui never hidden.", 0);

        renderWithPortraitAndLandscape(
                tosAndUmaFragment, "fre_metricsreportingdisabled_umadialog_childaccount");
    }

    private void launchFirstRunThroughCustomTab() throws TimeoutException {
        launchFirstRunThroughCustomTabPreNative();
        startNativeInitializationAndWait();
    }

    /**
     * Launch chrome through custom tab and trigger first run.
     */
    private void launchFirstRunThroughCustomTabPreNative() {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final Context context = instrumentation.getTargetContext();

        // Create an Intent that causes Chrome to run.
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, "https://test.com");

        // Start the FRE.
        final ActivityMonitor freMonitor =
                new ActivityMonitor(FirstRunActivity.class.getName(), null, false);
        instrumentation.addMonitor(freMonitor);
        // As we want to test on FirstRunActivity, which starts its lifecycle *before*
        // CustomTabActivity fully initialized, we'll launch the activity without the help of
        // CustomTabActivityTestRule (which waits until any tab is created).
        context.startActivity(intent);

        // Wait for the FRE to be ready to use.
        Activity activity =
                freMonitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        instrumentation.removeMonitor(freMonitor);

        mActivity = (FirstRunActivity) activity;
        CriteriaHelper.pollUiThread(
                () -> mActivity.getSupportFragmentManager().getFragments().size() > 0);

        mTosText = mActivity.findViewById(R.id.tos_and_privacy);
        mUmaCheckBox = mActivity.findViewById(R.id.send_report_checkbox);
        mAcceptButton = mActivity.findViewById(R.id.terms_accept);
        mLowerSpinner = mActivity.findViewById(R.id.progress_spinner);
        mCenterSpinner = mActivity.findViewById(R.id.progress_spinner_large);
        mPrivacyDisclaimer = mActivity.findViewById(R.id.privacy_disclaimer);
    }

    private void assertUIState(@FragmentState int fragmentState) {
        int tosVisibility = (fragmentState == FragmentState.NO_POLICY) ? View.VISIBLE : View.GONE;
        int spinnerVisibility = (fragmentState == FragmentState.LOADING) ? View.VISIBLE : View.GONE;
        int privacyVisibility =
                (fragmentState == FragmentState.HAS_POLICY) ? View.VISIBLE : View.GONE;
        int lowerSpinnerVisibility =
                (fragmentState == FragmentState.WAITING_UNTIL_NEXT_PAGE) ? View.VISIBLE : View.GONE;

        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                "Visibility of Loading spinner never reached test setting.",
                                mCenterSpinner.getVisibility(), Matchers.is(spinnerVisibility)));

        Assert.assertEquals("Visibility of ToS text is different than the test setting.",
                tosVisibility, mTosText.getVisibility());
        Assert.assertEquals("Visibility of Uma Check Box is different than the test setting.",
                tosVisibility, mUmaCheckBox.getVisibility());
        Assert.assertEquals("Visibility of accept button is different than the test setting.",
                tosVisibility, mAcceptButton.getVisibility());
        Assert.assertEquals("Visibility of lower spinner is different than the test setting.",
                lowerSpinnerVisibility, mLowerSpinner.getVisibility());
        Assert.assertEquals("Visibility of privacy disclaimer is different than the test setting.",
                privacyVisibility, mPrivacyDisclaimer.getVisibility());

        Assert.assertTrue("Uma Check Box should be checked.", mUmaCheckBox.isChecked());

        int expectedExitCount = fragmentState == FragmentState.HAS_POLICY ? 1 : 0;
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mExitCount, Matchers.is(expectedExitCount)));
    }

    private void startNativeInitializationAndWait() throws TimeoutException {
        Mockito.verify(mInitializer, Mockito.timeout(3000L))
                .handlePostNativeStartup(eq(true), mBrowserParts.capture());
        Mockito.doCallRealMethod()
                .when(mInitializer)
                .handlePostNativeStartup(anyBoolean(), any(BrowserParts.class));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mInitializer.handlePostNativeStartup(
                                /*isAsync*/ false, mBrowserParts.getValue()));
        mOnNativeInitializedHelper.waitForCallback("native never initialized.", 0);
    }

    /**
     * Asserts the speed histograms related to FirstRunAppRestrictions and TosAndUmaFirstRunFragment
     * are recorded correctly. Noting that with the current test setup, it is possible that the
     * FragmentInflationSpeedCheck might be flaky.
     *
     * TODO(https://crbug.com/1120859): Move to a different setup once this test file moves to
     * robolectric.
     *
     * @param didLoading If Welcome screen ever attempts to wait and load for policies.
     * @param appRestrictionMetricsState {@link SpeedComparedToInflation} for checks regarding
     *         {@link FirstRunAppRestrictionInfo#getHasAppRestriction(Callback)}
     * @param deviceOwnershipMetricsState {@link SpeedComparedToInflation} for checks regarding
     *         {@link EnterpriseInfo#getDeviceEnterpriseInfo(Callback)}}
     * @param policyCheckMetricsState {@link SpeedComparedToInflation} for checks regarding {@link
     *         PolicyService#isInitializationComplete()}
     */
    private void assertHistograms(boolean didLoading,
            @SpeedComparedToInflation int appRestrictionMetricsState,
            @SpeedComparedToInflation int deviceOwnershipMetricsState,
            @SpeedComparedToInflation int policyCheckMetricsState) {
        assertSingleHistogram("MobileFre.CctTos.LoadingDuration", didLoading);

        // WARNING: These two checks might be flaky with current test setup.
        assertSingleHistogram("MobileFre.FragmentInflationSpeed.FasterThanAppRestriction",
                appRestrictionMetricsState == SpeedComparedToInflation.SLOWER);
        assertSingleHistogram("MobileFre.FragmentInflationSpeed.SlowerThanAppRestriction",
                appRestrictionMetricsState == SpeedComparedToInflation.FASTER);

        assertSingleHistogram("MobileFre.CctTos.IsDeviceOwnedCheckSpeed2.FasterThanInflation",
                deviceOwnershipMetricsState == SpeedComparedToInflation.FASTER);
        assertSingleHistogram("MobileFre.CctTos.IsDeviceOwnedCheckSpeed2.SlowerThanInflation",
                deviceOwnershipMetricsState == SpeedComparedToInflation.SLOWER);

        assertSingleHistogram("MobileFre.CctTos.EnterprisePolicyCheckSpeed2.FasterThanInflation",
                policyCheckMetricsState == SpeedComparedToInflation.FASTER);
        assertSingleHistogram("MobileFre.CctTos.EnterprisePolicyCheckSpeed2.SlowerThanInflation",
                policyCheckMetricsState == SpeedComparedToInflation.SLOWER);
    }

    /**
     * Assert MobileFre.PolicyServiceInitDelayAfterNative.* is recorded correctly. The histogram
     * should be recorded when {@link PolicyLoadListener} is ready after native initialization.
     * @param recorded Whether the histogram should be recorded.
     * @param isPolicyFound Used to determine which suffix would have been used.
     */
    private void assertPolicyServiceInitDelayAfterNative(boolean recorded, boolean isPolicyFound) {
        assertSingleHistogram("MobileFre.PolicyServiceInitDelayAfterNative.WithPolicy2",
                recorded && isPolicyFound);
        assertSingleHistogram("MobileFre.PolicyServiceInitDelayAfterNative.WithoutPolicy2",
                recorded && !isPolicyFound);
    }

    private void assertSingleHistogram(String histogram, boolean recorded) {
        Assert.assertEquals("Histogram <" + histogram + "> is not recorded correctly.",
                recorded ? 1 : 0, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private void setAppRestrictionsMockNotInitialized() {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   mAppRestrictionsCallbacks.add(callback);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());

        Mockito.doAnswer(invocation -> {
                   Callback<Long> callback = invocation.getArgument(0);
                   mAppRestrictionsDurationCallbacks.add(callback);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getCompletionElapsedRealtimeMs(any());
    }

    private void setAppRestrictionsMockInitialized(boolean hasAppRestrictions) {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   callback.onResult(hasAppRestrictions);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());

        long resolvingTime = SystemClock.elapsedRealtime();
        Mockito.doAnswer(invocation -> {
                   Callback<Long> callback = invocation.getArgument(0);
                   callback.onResult(resolvingTime);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getCompletionElapsedRealtimeMs(any());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Callback<Boolean> callback : mAppRestrictionsCallbacks) {
                callback.onResult(hasAppRestrictions);
            }

            for (Callback<Long> callback : mAppRestrictionsDurationCallbacks) {
                callback.onResult(resolvingTime);
            }
        });
    }

    private void setPolicyServiceMockNotInitialized() {
        Mockito.when(mPolicyService.isInitializationComplete()).thenReturn(false);
        Mockito.doAnswer(invocation -> {
                   PolicyService.Observer observer = invocation.getArgument(0);
                   mPolicyServiceObservers.add(observer);
                   return null;
               })
                .when(mPolicyService)
                .addObserver(any());
    }

    private void setPolicyServiceMockInitializedWithDialogEnabled(boolean cctTosDialogEnabled) {
        setMockCctTosDialogEnabled(cctTosDialogEnabled);

        Mockito.when(mPolicyService.isInitializationComplete()).thenReturn(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (PolicyService.Observer observer : mPolicyServiceObservers) {
                observer.onPolicyServiceInitialized();
            }
        });
        mPolicyServiceObservers.clear();
    }

    private void setMockCctTosDialogEnabled(boolean cctTosDialogEnabled) {
        Mockito.when(mFirstRunUtils.getCctTosDialogEnabled()).thenReturn(cctTosDialogEnabled);
    }

    private void setEnterpriseInfoInitializedWithDeviceOwner(boolean hasDeviceOwner) {
        mFakeEnterpriseInfo.initialize(new EnterpriseInfo.OwnedState(hasDeviceOwner, false));
    }

    private void setMetricsReportDisabled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);
        Assert.assertFalse("Crash report should be disabled by shared preference.",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
    }

    private void renderWithPortraitAndLandscape(View tosAndUmaFragmentView, String testPrefix)
            throws Exception {
        mRenderTestRule.render(tosAndUmaFragmentView, testPrefix + "_portrait");

        setDeviceOrientation(tosAndUmaFragmentView, Configuration.ORIENTATION_LANDSCAPE);
        mRenderTestRule.render(tosAndUmaFragmentView, testPrefix + "_landscape");

        setDeviceOrientation(tosAndUmaFragmentView, Configuration.ORIENTATION_PORTRAIT);
        mRenderTestRule.render(tosAndUmaFragmentView, testPrefix + "_portrait");
    }

    private void setDeviceOrientation(View tosAndUmaFragmentView, int orientation) {
        // TODO(https://crbug.com/1133789): This function is copied mostly copied from
        // TabUiTestHelper#rotateDeviceToOrientation. Merge / move these two test functions if
        // applicable.
        if (mActivity.getResources().getConfiguration().orientation == orientation) return;
        assertTrue(orientation == Configuration.ORIENTATION_LANDSCAPE
                || orientation == Configuration.ORIENTATION_PORTRAIT);

        boolean isLandscape = orientation == Configuration.ORIENTATION_LANDSCAPE;
        mActivity.setRequestedOrientation(isLandscape ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                                                      : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivity.getResources().getConfiguration().orientation, is(orientation));
            Criteria.checkThat(tosAndUmaFragmentView.getWidth() > tosAndUmaFragmentView.getHeight(),
                    is(isLandscape));
        });
    }
}
