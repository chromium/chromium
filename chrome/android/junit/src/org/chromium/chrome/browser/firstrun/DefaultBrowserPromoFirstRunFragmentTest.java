// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.UiAndroidFeatures;
import org.chromium.ui.base.WindowAndroid;

/** Robolectric tests for {@link DefaultBrowserPromoFirstRunFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DEFAULT_BROWSER_PROMO_FRE})
public class DefaultBrowserPromoFirstRunFragmentTest {

    /**
     * * Custom version of the fragment to allow manual injection of the delegate since we aren't
     * running inside a real FirstRunActivity.
     */
    public static class CustomDefaultBrowserPromoFirstRunFragment
            extends DefaultBrowserPromoFirstRunFragment {
        private FirstRunPageDelegate mFirstRunPageDelegate;

        @Override
        public FirstRunPageDelegate getPageDelegate() {
            return mFirstRunPageDelegate;
        }

        void setPageDelegate(FirstRunPageDelegate delegate) {
            mFirstRunPageDelegate = delegate;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FirstRunPageDelegate mMockDelegate;
    @Mock private DefaultBrowserPromoUtils mMockUtils;
    @Mock private ProfileProvider mMockProfileProvider;
    @Mock private Profile mMockProfile;
    @Mock private Tracker mMockTracker;
    @Mock private WindowAndroid mMockWindow;

    private FragmentScenario<CustomDefaultBrowserPromoFirstRunFragment> mScenario;

    @Before
    public void setUp() {
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockUtils);

        OneshotSupplierImpl<ProfileProvider> profileSupplier = new OneshotSupplierImpl<>();
        profileSupplier.set(mMockProfileProvider);

        when(mMockProfileProvider.getOriginalProfile()).thenReturn(mMockProfile);
        when(mMockDelegate.getProfileProviderSupplier()).thenReturn(profileSupplier);
        when(mMockDelegate.getWindowAndroid()).thenReturn(mMockWindow);
        TrackerFactory.setTrackerForTests(mMockTracker);

        FeatureOverrides.newBuilder()
                .enable(UiAndroidFeatures.REQUIRE_LEADING_IN_TEXT_VIEW_WITH_LEADING)
                .apply();
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    private void launchFragment(String armValue) {
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.DEFAULT_BROWSER_PROMO_FRE)
                .param(DefaultBrowserPromoFirstRunFragment.FRE_PROMO_ARM, armValue)
                .apply();

        // LaunchInContainer creates a minimal activity and attaches our fragment to it and
        // moves the fragment through all the lifecycles (onAttach -> onCreate -> onCreateView ->
        // onResume).
        mScenario =
                FragmentScenario.launchInContainer(
                        CustomDefaultBrowserPromoFirstRunFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                CustomDefaultBrowserPromoFirstRunFragment fragment =
                                        new CustomDefaultBrowserPromoFirstRunFragment();
                                // setPageDelegate before onResume is called to avoid
                                // NullPointerErrors.
                                fragment.setPageDelegate(mMockDelegate);
                                return fragment;
                            }
                        });
    }

    @Test
    public void testOnResume_DirectInvocationArm_TriggersRMDOnceThenAdvancesToNextPage() {
        when(mMockUtils.prepareLaunchPromoIfNeeded(
                        any(),
                        eq(mMockWindow),
                        eq(mMockTracker),
                        eq(DefaultBrowserPromoEntryPoint.FRE)))
                .thenReturn(true);

        when(mMockDelegate.getPromoRoleManagerDialogTriggered())
                // For the first onResume (RMD direct invocation).
                .thenReturn(false)
                // Inside triggerRoleManagerDialog called from that onResume call.
                .thenReturn(false)
                // For the second onResume when the RMD is dismissed.
                .thenReturn(true);

        launchFragment(DefaultBrowserPromoFirstRunFragment.RMD_DIRECT_INVOCATION);

        mScenario.onFragment(
                fragment -> {
                    Activity activity = fragment.getActivity();

                    // Verify RMD was triggered.
                    verify(mMockUtils, times(1))
                            .prepareLaunchPromoIfNeeded(
                                    activity,
                                    mMockWindow,
                                    mMockTracker,
                                    DefaultBrowserPromoEntryPoint.FRE);

                    // Verify we haven't advanced yet.
                    verify(mMockDelegate, never()).advanceToNextPage();

                    // Second onResume: Simulate the user returning from the Role Manager Dialog.
                    fragment.onResume();

                    // Verify that because mDialogHasTriggered is now true, we now advance.
                    verify(mMockDelegate, times(1)).advanceToNextPage();
                });
    }

    @Test(expected = AssertionError.class)
    public void testOnResume_RmdNotTriggered_ThrowsAssertionError() {
        // Set didTrigger to false.
        when(mMockUtils.prepareLaunchPromoIfNeeded(
                        any(),
                        eq(mMockWindow),
                        eq(mMockTracker),
                        eq(DefaultBrowserPromoEntryPoint.FRE)))
                .thenReturn(false);

        // This call will hit 'assert false' and throw an AssertionError.
        launchFragment(DefaultBrowserPromoFirstRunFragment.RMD_DIRECT_INVOCATION);
    }

    @Test
    public void testArm2_InflatesPrimer_AndContinueTriggersRMD() {
        // Mock the RMD trigger to be successful.
        when(mMockUtils.prepareLaunchPromoIfNeeded(
                        any(),
                        eq(mMockWindow),
                        eq(mMockTracker),
                        eq(DefaultBrowserPromoEntryPoint.FRE)))
                .thenReturn(true);

        when(mMockDelegate.getPromoRoleManagerDialogTriggered())
                // For the first onResume (when the primer is shown).
                .thenReturn(false)
                // Inside triggerRoleManagerDialog called by the button click.
                .thenReturn(false)
                // For the second onResume when the RMD is dismissed.
                .thenReturn(true);

        launchFragment(DefaultBrowserPromoFirstRunFragment.PRIMER_NO_INSTRUCTIONS);

        mScenario.onFragment(
                fragment -> {
                    Activity activity = fragment.getActivity();

                    // Verify UI is inflated.
                    FrameLayout root = (FrameLayout) fragment.getView();
                    Assert.assertNotNull("Root view should not be null.", root);
                    Assert.assertEquals(
                            "Should have inflated the primer view.", 1, root.getChildCount());

                    // Find the Continue button from the custom view.
                    DefaultBrowserPromoFirstRunView primerView =
                            (DefaultBrowserPromoFirstRunView) root.getChildAt(0);
                    View continueButton = primerView.getContinueButtonView();

                    // Click the Continue button.
                    continueButton.performClick();

                    // Verify the Fragment told the delegate to record the ACCEPTED metric.
                    verify(mMockDelegate)
                            .recordFreProgressHistogram(
                                    MobileFreProgress.DEFAULT_BROWSER_PROMO_ACCEPTED);

                    // Verify RMD was triggered.
                    verify(mMockUtils, times(1))
                            .prepareLaunchPromoIfNeeded(
                                    activity,
                                    mMockWindow,
                                    mMockTracker,
                                    DefaultBrowserPromoEntryPoint.FRE);

                    // Verify we haven't advanced yet.
                    verify(mMockDelegate, never()).advanceToNextPage();

                    // Second onResume: Simulate the user returning from the Role Manager Dialog.
                    fragment.onResume();

                    // Verify it advanced to the next page.
                    verify(mMockDelegate, times(1)).advanceToNextPage();
                });
    }

    @Test
    public void testArm2_DismissButton_AdvancesToNextPage() {
        launchFragment(DefaultBrowserPromoFirstRunFragment.PRIMER_NO_INSTRUCTIONS);

        mScenario.onFragment(
                fragment -> {
                    // #launchFragment should have inflated the layout.
                    FrameLayout root = (FrameLayout) fragment.getView();
                    DefaultBrowserPromoFirstRunView primerView =
                            (DefaultBrowserPromoFirstRunView) root.getChildAt(0);
                    View dismissButton = primerView.getDismissButtonView();

                    // Click the Dismiss button.
                    dismissButton.performClick();

                    // Verify the Fragment told the delegate to record the REJECTED metric.
                    verify(mMockDelegate)
                            .recordFreProgressHistogram(
                                    MobileFreProgress.DEFAULT_BROWSER_PROMO_REJECTED);

                    // Verify that we advance to the next page immediately without triggering the
                    // RMD.
                    verify(mMockDelegate, times(1)).advanceToNextPage();
                    verify(mMockUtils, never())
                            .prepareLaunchPromoIfNeeded(any(), any(), any(), anyInt());
                });
    }
}
