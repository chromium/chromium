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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.policy.PolicyService;
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
    private final List<Callback<Boolean>> mAppRestrictonsCallbacks = new ArrayList<>();
    private final List<Callback<EnterpriseInfo.OwnedState>> mOwnedStateCallbacks =
            new ArrayList<>();
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
            public void onAcceptTermsOfService() {}

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
        if (mActivity != null) mActivity.finish();
    }

    @Test
    @SmallTest
    public void testNoRestriction() {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setAppRestrictionsMockInitialized(false);
        assertUIState(FragmentState.NO_POLICY);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.LoadingDuration"));
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
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.LoadingDuration"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.EnterprisePolicyCheckSpeed.SlowerThanInflation"));
    }

    @Test
    @SmallTest
    public void testNotOwnedDevice() {
        setAppRestrictionsMockInitialized(true);
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setEnterpriseInfoInitializedWithDeviceOwner(false);
        assertUIState(FragmentState.NO_POLICY);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.LoadingDuration"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"));
    }

    @Test
    @SmallTest
    public void testNotOwnedDevice_beforeInflation() {
        setAppRestrictionsMockInitialized(true);
        setEnterpriseInfoInitializedWithDeviceOwner(false);

        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.NO_POLICY);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.LoadingDuration"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.FasterThanInflation"));
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

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.LoadingDuration"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"));
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

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.LoadingDuration"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"));
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

        // assertUIState will verify that exit was not called a second time.
        setAppRestrictionsMockInitialized(true);
        assertUIState(FragmentState.HAS_POLICY);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.LoadingDuration"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"));
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
        mAcceptButton = mActivity.findViewById(R.id.tos_and_privacy);
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

    private void waitUntilNativeLoaded() {
        CriteriaHelper.pollUiThread(
                (() -> mActivity.isNativeSideIsInitializedForTest()), "native never initialized.");
    }

    private void setAppRestrictionsMockNotInitialized() {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   mAppRestrictonsCallbacks.add(callback);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());
    }

    private void setAppRestrictionsMockInitialized(boolean hasAppRestrictons) {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   callback.onResult(hasAppRestrictons);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Callback<Boolean> callback : mAppRestrictonsCallbacks) {
                callback.onResult(hasAppRestrictons);
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
