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

import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/**
 * JUnit tests of the class {@link PrivacyGuidePagerAdapter}
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class PrivacyGuidePagerAdapterTest {
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{false, false, false}, {false, false, true},
                {false, true, false}, {false, true, true}, {true, false, false},
                {true, false, true}, {true, true, false}, {true, true, true}});
    }
    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameter(0)
    public boolean mShouldDisplayHistorySync;
    @Parameter(1)
    public boolean mShouldDisplaySafeBrowsing;
    @Parameter(2)
    public boolean mShouldDisplayCookies;

    private StepDisplayHandler mStepDisplayHandler;
    private FragmentScenario mScenario;
    private Fragment mFragment;
    private PrivacyGuidePagerAdapter mPagerAdapter;

    @Before
    public void setUp() {
        mScenario = FragmentScenario.launchInContainer(
                Fragment.class, Bundle.EMPTY, R.style.Theme_MaterialComponents);
        mScenario.onFragment(fragment -> mFragment = fragment);
        initPagerAdapterWithState();
    }

    @After
    public void tearDown() {
        mScenario.close();
    }

    private void initPagerAdapterWithState() {
        mStepDisplayHandler = new StepDisplayHandler() {
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
        };
        mPagerAdapter = new PrivacyGuidePagerAdapter(
                mFragment, mStepDisplayHandler, PrivacyGuideFragment.ALL_FRAGMENT_TYPE_ORDER);
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
        Assert.assertEquals("History Sync step displayed incorrectly", mShouldDisplayHistorySync,
                fragmentClassSet.contains(HistorySyncFragment.class));
        Assert.assertEquals("Safe Browsing step displayed incorrectly", mShouldDisplaySafeBrowsing,
                fragmentClassSet.contains(SafeBrowsingFragment.class));
        Assert.assertEquals("Cookies step displayed incorrectly", mShouldDisplayCookies,
                fragmentClassSet.contains(CookiesFragment.class));
    }
}
