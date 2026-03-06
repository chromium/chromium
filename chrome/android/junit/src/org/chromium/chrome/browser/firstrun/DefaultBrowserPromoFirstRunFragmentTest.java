// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

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

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint;
import org.chromium.components.feature_engagement.Tracker;
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

    private static final String RMD_DIRECT_INVOCATION = "rmd_direct_invocation";
    private static final String PRIMER_NO_INSTRUCTIONS = "primer_no_instructions";

    @Before
    public void setUp() {
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockUtils);

        OneshotSupplierImpl<ProfileProvider> profileSupplier = new OneshotSupplierImpl<>();
        profileSupplier.set(mMockProfileProvider);

        when(mMockProfileProvider.getOriginalProfile()).thenReturn(mMockProfile);
        when(mMockDelegate.getProfileProviderSupplier()).thenReturn(profileSupplier);
        when(mMockDelegate.getWindowAndroid()).thenReturn(mMockWindow);
        TrackerFactory.setTrackerForTests(mMockTracker);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    @Test
    public void testOnResume_DirectInvocationArm_TriggersRMDOnceThenAdvancesToNextPage() {
        // Set the arm to something else so the first onResume called via launchInContainer does
        // nothing.
        ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting("none");

        // LaunchInContainer creates a minimal activity and attaches our fragment to it and
        // moves the fragment through all the lifecycles (onAttach -> onCreate -> onCreateView ->
        // onResume).
        mScenario =
                FragmentScenario.launchInContainer(CustomDefaultBrowserPromoFirstRunFragment.class);

        // The perform() action is called as soon as the Fragment is ready and in the RESUMED
        // state.
        mScenario.onFragment(
                fragment -> {
                    fragment.setPageDelegate(mMockDelegate);

                    // Now that we set the delegate, we know it won't be null so we can set the arm
                    // to the correct value.
                    ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting(
                            RMD_DIRECT_INVOCATION);

                    Activity activity = fragment.getActivity();

                    when(mMockUtils.prepareLaunchPromoIfNeeded(
                                    activity,
                                    mMockWindow,
                                    mMockTracker,
                                    DefaultBrowserPromoEntryPoint.FRE))
                            .thenReturn(true);

                    // First onResume: Manually re-trigger to simulate entering the fragment and
                    // showing the RMD.
                    fragment.onResume();

                    // The RMD should have been triggered once.
                    verify(mMockUtils, times(1))
                            .prepareLaunchPromoIfNeeded(
                                    activity,
                                    mMockWindow,
                                    mMockTracker,
                                    DefaultBrowserPromoEntryPoint.FRE);

                    // Verify we haven't advanced yet.
                    verify(mMockDelegate, never()).advanceToNextPage();

                    // 2. Second onResume: Simulate the user returning from the Role Manager Dialog.
                    fragment.onResume();

                    // Verify that because mHasTriggered is now true, we now advance.
                    verify(mMockDelegate, times(1)).advanceToNextPage();
                });
    }

    @Test(expected = AssertionError.class)
    public void testOnResume_RmdNotTriggered_ThrowsAssertionError() {
        ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting("none");
        mScenario =
                FragmentScenario.launchInContainer(CustomDefaultBrowserPromoFirstRunFragment.class);

        mScenario.onFragment(
                fragment -> {
                    fragment.setPageDelegate(mMockDelegate);
                    ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting(
                            RMD_DIRECT_INVOCATION);

                    Activity activity = fragment.getActivity();

                    // Set didTrigger to false.
                    when(mMockUtils.prepareLaunchPromoIfNeeded(
                                    activity,
                                    mMockWindow,
                                    mMockTracker,
                                    DefaultBrowserPromoEntryPoint.FRE))
                            .thenReturn(false);

                    // This call will hit 'assert false' and throw an AssertionError.
                    fragment.onResume();
                });
    }

    @Test
    public void testArm2_InflatesPrimer_AndContinueTriggersRMD() {
        // Set to arm 2.
        ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting(PRIMER_NO_INSTRUCTIONS);

        mScenario =
                FragmentScenario.launchInContainer(CustomDefaultBrowserPromoFirstRunFragment.class);

        mScenario.onFragment(
                fragment -> {
                    fragment.setPageDelegate(mMockDelegate);
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

                    // Mock the RMD trigger to be successful.
                    when(mMockUtils.prepareLaunchPromoIfNeeded(
                                    activity,
                                    mMockWindow,
                                    mMockTracker,
                                    DefaultBrowserPromoEntryPoint.FRE))
                            .thenReturn(true);

                    // Click the Continue button.
                    continueButton.performClick();

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
        // Set arm to "none" so initial onCreateView doesn't call updateView (which calls
        // getPageDelegate).
        ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting("none");

        // launchInContainer runs a full lifecycle immediately.
        mScenario =
                FragmentScenario.launchInContainer(CustomDefaultBrowserPromoFirstRunFragment.class);

        mScenario.onFragment(
                fragment -> {
                    fragment.setPageDelegate(mMockDelegate);
                    // Now set the real arm since we set the mock delegate.
                    ChromeFeatureList.sDefaultBrowserPromoFreArm.setForTesting(
                            PRIMER_NO_INSTRUCTIONS);

                    // onConfigurationChanged calls rootView.removeAllViews() and then updateView.
                    // By calling updateView manually, we force the fragment to actually inflate the
                    // layout.
                    fragment.onConfigurationChanged(fragment.getResources().getConfiguration());

                    FrameLayout root = (FrameLayout) fragment.getView();
                    DefaultBrowserPromoFirstRunView primerView =
                            (DefaultBrowserPromoFirstRunView) root.getChildAt(0);
                    View dismissButton = primerView.getDismissButtonView();

                    // Click the Dismiss button.
                    dismissButton.performClick();

                    // Verify that we advance to the next page immediately without triggering the
                    // RMD.
                    verify(mMockDelegate, times(1)).advanceToNextPage();
                    verify(mMockUtils, never())
                            .prepareLaunchPromoIfNeeded(any(), any(), any(), anyInt());
                });
    }
}
