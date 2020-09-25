// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
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
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.policy.PolicyService;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Test for first run activity and {@link TosAndUmaFirstRunFragmentWithEnterpriseSupport}.
 * For the outside signals that used in this test so that the verification is focusing on the
 * workflow and UI transition.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TosAndUmaFirstRunFragmentWithEnterpriseSupportTest {
    @IntDef({FragmentState.LOADING, FragmentState.NO_POLICY, FragmentState.HAS_POLICY})
    @Retention(RetentionPolicy.SOURCE)
    @interface FragmentState {
        int LOADING = 0;
        int NO_POLICY = 1;
        int HAS_POLICY = 2;
    }

    @IntDef({SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.FASTER,
            SpeedComparedToInflation.SLOWER})
    @Retention(RetentionPolicy.SOURCE)
    @interface SpeedComparedToInflation {
        int NOT_RECORDED = 0;
        int FASTER = 1;
        int SLOWER = 2;
    }

    @Rule
    public DisableAnimationsTestRule mDisableAnimationsTestRule = new DisableAnimationsTestRule();

    @Mock
    public FirstRunAppRestrictionInfo mMockAppRestrictionInfo;
    @Mock
    public PolicyService mPolicyService;
    @Mock
    public FirstRunUtils.Natives mFirstRunUtils;
    @Mock
    public EnterpriseInfo mMockEnterpriseInfo;

    private FirstRunActivity mActivity;
    private final List<PolicyService.Observer> mPolicyServiceObservers = new ArrayList<>();
    private final List<Callback<Boolean>> mAppRestrictionsCallbacks = new ArrayList<>();
    private final List<Callback<Long>> mAppRestrictionsDurationCallbacks = new ArrayList<>();
    private final List<Callback<EnterpriseInfo.OwnedState>> mOwnedStateCallbacks =
            new ArrayList<>();
    private final CallbackHelper mAcceptTosCallbackHelper = new CallbackHelper();
    private int mExitCount;

    private View mTosText;
    private View mAcceptButton;
    private View mLargeSpinner;
    private CheckBox mUmaCheckBox;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Assert.assertFalse(
                CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE));

        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(mMockAppRestrictionInfo);
        ToSAndUMAFirstRunFragment.setShowUmaCheckBoxForTesting(true);
        PolicyServiceFactory.setPolicyServiceForTest(mPolicyService);
        FirstRunUtilsJni.TEST_HOOKS.setInstanceForTesting(mFirstRunUtils);
        EnterpriseInfo.setInstanceForTest(mMockEnterpriseInfo);

        setAppRestrictionsMockNotInitialized();
        setPolicyServiceMockNotInitialized();
        setEnterpriseInfoNotInitialized();

        // TODO(https://crbug.com/1113229): Rework this to not depend on {@link FirstRunActivity}
        // implementation details.
        mExitCount = 0;
        FirstRunActivity.setObserverForTest(new FirstRunActivity.FirstRunActivityObserver() {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {}

            @Override
            public void onAcceptTermsOfService() {
                mAcceptTosCallbackHelper.notifyCalled();
            }

            @Override
            public void onJumpToPage(int position) {}

            @Override
            public void onUpdateCachedEngineName() {}

            @Override
            public void onAbortFirstRunExperience() {}

            @Override
            public void onExitFirstRun() {
                mExitCount++;
            }
        });
    }

    @After
    public void tearDown() {
        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(null);
        ToSAndUMAFirstRunFragment.setShowUmaCheckBoxForTesting(false);
        PolicyServiceFactory.setPolicyServiceForTest(null);
        FirstRunUtilsJni.TEST_HOOKS.setInstanceForTesting(mFirstRunUtils);
        EnterpriseInfo.setInstanceForTest(null);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING, false);
        if (mActivity != null) mActivity.finish();
    }

    @Test
    @SmallTest
    public void testNoRestriction() {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.NOT_RECORDED);

        // Try to accept Tos.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) mAcceptButton::performClick);
        Assert.assertTrue("Crash report should be enabled.",
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testNoRestriction_BeforeInflation() {
        setAppRestrictionsMockInitialized(false);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(false, SpeedComparedToInflation.FASTER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.NOT_RECORDED);
    }

    @Test
    @SmallTest
    // TODO(crbug.com/1120859): Test the policy check when native initializes before inflation.
    // This will be possible when FragmentScenario is available.
    public void testDialogEnabled() {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(true);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.SLOWER);

        // Try to accept Tos.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) mAcceptButton::performClick);
        Assert.assertTrue("Crash report should be enabled.",
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testDialogEnabled_BeforeAppRestrictions() {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // When policy is loaded on fully managed device, we don't need app restriction.
        setPolicyServiceMockInitializedWithDialogEnabled(true);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.NOT_RECORDED,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.SLOWER);
    }

    @Test
    @SmallTest
    public void testDialogDisabled_NoRestriction() {
        setPolicyServiceMockInitializedWithDialogEnabled(false);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED, SpeedComparedToInflation.SLOWER);
    }

    @Test
    @SmallTest
    public void testNotOwnedDevice() {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(false);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.NOT_RECORDED);

        // Try to accept Tos.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) mAcceptButton::performClick);
        Assert.assertTrue("Crash report should be enabled.",
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testNotOwnedDevice_beforeInflation() {
        setAppRestrictionsMockInitialized(true);
        setEnterpriseInfoInitializedWithDeviceOwner(false);

        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(false, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.FASTER,
                SpeedComparedToInflation.NOT_RECORDED);
    }

    @Test
    @SmallTest
    public void testOwnedDevice_NoRestriction() {
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.FASTER,
                SpeedComparedToInflation.NOT_RECORDED);
    }

    @Test
    @SmallTest
    public void testOwnedDevice_NoPolicy() {
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(true);
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(true);
        assertUIState(FragmentState.NO_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.FASTER,
                SpeedComparedToInflation.SLOWER);
    }

    @Test
    @SmallTest
    public void testSkip_DeviceOwnedThenDialogPolicy() {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(true);
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        Assert.assertFalse("Crash report should not be enabled.",
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testSkip_DialogPolicyThenDeviceOwned() {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(true);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.FASTER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        Assert.assertFalse("Crash report should not be enabled.",
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testSkip_LateAppRestrictions() {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setPolicyServiceMockInitializedWithDialogEnabled(false);
        assertUIState(FragmentState.LOADING);

        // Skip should happen without app restrictions being completed.
        setEnterpriseInfoInitializedWithDeviceOwner(true);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.NOT_RECORDED,
                SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.SLOWER);

        // assertUIState will verify that exit was not called a second time.
        setAppRestrictionsMockInitialized(true);
        assertUIState(FragmentState.HAS_POLICY);

        assertHistograms(true, SpeedComparedToInflation.SLOWER, SpeedComparedToInflation.SLOWER,
                SpeedComparedToInflation.SLOWER);
        Assert.assertFalse("Crash report should not be enabled.",
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    @SmallTest
    public void testNullOwnedState() {
        setAppRestrictionsMockInitialized(true);
        setPolicyServiceMockInitializedWithDialogEnabled(false);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        // Null means loading checking if the device is owned failed. This should be treated the
        // same as not being owned, and no skipping should occur.
        setEnterpriseInfoInitializedWithOwnedState(null);
        assertUIState(FragmentState.NO_POLICY);
    }

    @Test
    @SmallTest
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
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser());
    }

    /**
     * Launch chrome through custom tab and trigger first run.
     */
    private void launchFirstRunThroughCustomTab() {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final Context context = instrumentation.getTargetContext();

        // Create an Intent that causes Chrome to run.
        Intent intent =
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, "https://test.com");

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

        // Force this to happen now to try to make the tests more deterministic. Ideally the tests
        // could control when this happens and test for difference sequences.
        waitUntilNativeLoaded();

        mTosText = mActivity.findViewById(R.id.tos_and_privacy);
        mUmaCheckBox = mActivity.findViewById(R.id.send_report_checkbox);
        mAcceptButton = mActivity.findViewById(R.id.terms_accept);
        mLargeSpinner = mActivity.findViewById(R.id.progress_spinner_large);
    }

    private void assertUIState(@FragmentState int fragmentState) {
        int tosVisibility = (fragmentState == FragmentState.NO_POLICY) ? View.VISIBLE : View.GONE;
        int spinnerVisibility = (fragmentState == FragmentState.LOADING) ? View.VISIBLE : View.GONE;

        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                "Visibility of Loading spinner never reached test setting.",
                                mLargeSpinner.getVisibility(), Matchers.is(spinnerVisibility)));

        Assert.assertEquals("Visibility of ToS text is different than the test setting.",
                tosVisibility, mTosText.getVisibility());
        Assert.assertEquals("Visibility of Uma Check Box is different than the test setting.",
                tosVisibility, mUmaCheckBox.getVisibility());
        Assert.assertEquals("Visibility of accept button is different than the test setting.",
                tosVisibility, mAcceptButton.getVisibility());

        Assert.assertTrue("Uma Check Box should be checked.", mUmaCheckBox.isChecked());

        int expectedExitCount = fragmentState == FragmentState.HAS_POLICY ? 1 : 0;
        Assert.assertEquals(expectedExitCount, mExitCount);
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

        assertSingleHistogram("MobileFre.CctTos.IsDeviceOwnedCheckSpeed.FasterThanInflation",
                deviceOwnershipMetricsState == SpeedComparedToInflation.FASTER);
        assertSingleHistogram("MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation",
                deviceOwnershipMetricsState == SpeedComparedToInflation.SLOWER);

        assertSingleHistogram("MobileFre.CctTos.EnterprisePolicyCheckSpeed.FasterThanInflation",
                policyCheckMetricsState == SpeedComparedToInflation.FASTER);
        assertSingleHistogram("MobileFre.CctTos.EnterprisePolicyCheckSpeed.SlowerThanInflation",
                policyCheckMetricsState == SpeedComparedToInflation.SLOWER);
    }

    private void assertSingleHistogram(String histogram, boolean recorded) {
        Assert.assertEquals("Histogram <" + histogram + "> is not recorded correctly.",
                recorded ? 1 : 0, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private void waitUntilNativeLoaded() {
        CriteriaHelper.pollUiThread(
                (() -> mActivity.isNativeSideIsInitializedForTest()), "native never initialized.");
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

    private void setEnterpriseInfoNotInitialized() {
        Mockito.doAnswer(invocation -> {
                   Callback<EnterpriseInfo.OwnedState> callback = invocation.getArgument(0);
                   mOwnedStateCallbacks.add(callback);
                   return null;
               })
                .when(mMockEnterpriseInfo)
                .getDeviceEnterpriseInfo(any());
    }

    private void setEnterpriseInfoInitializedWithDeviceOwner(boolean hasDeviceOwner) {
        setEnterpriseInfoInitializedWithOwnedState(
                new EnterpriseInfo.OwnedState(hasDeviceOwner, false));
    }

    private void setEnterpriseInfoInitializedWithOwnedState(EnterpriseInfo.OwnedState ownedState) {
        Mockito.doAnswer(invocation -> {
                   Callback<EnterpriseInfo.OwnedState> callback = invocation.getArgument(0);
                   callback.onResult(ownedState);
                   return null;
               })
                .when(mMockEnterpriseInfo)
                .getDeviceEnterpriseInfo(any());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Callback<EnterpriseInfo.OwnedState> callback : mOwnedStateCallbacks) {
                callback.onResult(ownedState);
            }
        });
    }
}
