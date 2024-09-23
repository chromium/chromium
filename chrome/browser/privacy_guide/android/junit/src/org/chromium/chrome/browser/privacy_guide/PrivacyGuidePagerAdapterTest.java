// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/** JUnit tests of the class {@link PrivacyGuidePagerAdapter} */
@RunWith(ParameterizedRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3,
    ChromeFeatureList.PRIVACY_GUIDE_PRELOAD_ANDROID
})
public class PrivacyGuidePagerAdapterTest {
    public static Collection<Object[]> generateBooleanCombinations(int nElements) {
        Collection<Object[]> result = new ArrayList<>();
        generate(result, new Boolean[nElements], 0);
        return result;
    }

    private static void generate(Collection<Object[]> result, Boolean[] current, int index) {
        // Base case
        if (index == current.length) {
            // Row is filled so add the current combination to the result and return
            result.add(current.clone());
            return;
        }

        // Set current index to false and recurse
        current[index] = false;
        generate(result, current, index + 1);
        // Set current index to true and recurse
        current[index] = true;
        generate(result, current, index + 1);
    }

    @Parameters
    public static Collection<Object[]> data() {
        int nElements = 5; // Number of elements in each combination
        return generateBooleanCombinations(nElements);
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameter(0)
    public boolean mShouldDisplayHistorySync;

    @Parameter(1)
    public boolean mShouldDisplaySafeBrowsing;

    @Parameter(2)
    public boolean mShouldDisplayCookies;

    @Parameter(3)
    public boolean mShouldDisplayPreload;

    @Parameter(4)
    public boolean mShouldDisplayAdTopics;

    private StepDisplayHandler mStepDisplayHandler;
    private FragmentScenario mScenario;
    private Fragment mFragment;
    private PrivacyGuidePagerAdapter mPagerAdapter;

    @Before
    public void setUp() {
        mScenario =
                FragmentScenario.launchInContainer(
                        Fragment.class, Bundle.EMPTY, R.style.Theme_MaterialComponents);
        mScenario.onFragment(fragment -> mFragment = fragment);
        initPagerAdapterWithState();
    }

    @After
    public void tearDown() {
        mScenario.close();
    }

    private void initPagerAdapterWithState() {
        mStepDisplayHandler =
                new StepDisplayHandler() {
                    @Override
                    public boolean shouldDisplayHistorySync() {
                        return mShouldDisplayHistorySync;
                    }

                    @Override
                    public boolean shouldDisplaySafeBrowsing() {
                        return mShouldDisplaySafeBrowsing;
                    }

                    @Override
                    public boolean shouldDisplayCookies() {
                        return mShouldDisplayCookies;
                    }

                    @Override
                    public boolean shouldDisplayPreload() {
                        return mShouldDisplayPreload;
                    }

                    @Override
                    public boolean shouldDisplayAdTopics() {
                        return mShouldDisplayAdTopics;
                    }
                };
        mPagerAdapter =
                new PrivacyGuidePagerAdapter(
                        mFragment,
                        mStepDisplayHandler,
                        PrivacyGuideFragment.ALL_FRAGMENT_TYPE_ORDER_PG3);
    }

    private Set<Class> getDisplayedFragmentClasses() {
        Set<Class> fragmentClassSet = new HashSet<>();
        for (int i = 0; i < mPagerAdapter.getItemCount(); i++) {
            fragmentClassSet.add(mPagerAdapter.createFragment(i).getClass());
        }
        return fragmentClassSet;
    }

    @Test
    public void testFragmentsDisplayed() {
        Set<Class> fragmentClassSet = getDisplayedFragmentClasses();
        Assert.assertTrue(fragmentClassSet.contains(MSBBFragment.class));
        Assert.assertEquals(
                "History Sync step displayed incorrectly",
                mShouldDisplayHistorySync,
                fragmentClassSet.contains(HistorySyncFragment.class));
        Assert.assertEquals(
                "Cookies step displayed incorrectly",
                mShouldDisplayCookies,
                fragmentClassSet.contains(CookiesFragment.class));
        Assert.assertEquals(
                "Safe Browsing step displayed incorrectly",
                mShouldDisplaySafeBrowsing,
                fragmentClassSet.contains(SafeBrowsingFragment.class));
        Assert.assertTrue(fragmentClassSet.contains(SearchSuggestionsFragment.class));
        Assert.assertEquals(
                "Preload step displayed incorrectly",
                mShouldDisplayPreload,
                fragmentClassSet.contains(PreloadFragment.class));
        Assert.assertEquals(
                "Ad Topics step displayed incorrectly",
                mShouldDisplayAdTopics,
                fragmentClassSet.contains(AdTopicsFragment.class));
    }
}
