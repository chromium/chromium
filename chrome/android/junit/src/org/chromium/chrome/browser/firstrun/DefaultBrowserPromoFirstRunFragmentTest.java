// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
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
                            "rmd_direct_invocation");

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
                            "rmd_direct_invocation");

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
}
