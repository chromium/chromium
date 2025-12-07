// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SafeBrowsingSettingsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SafeBrowsingSettingsFragmentUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfile;
    @Mock private SafeBrowsingBridge.Natives mSafeBrowsingBridgeJni;

    private TestActivity mActivity;
    private SafeBrowsingSettingsFragment mSettings;
    private RadioButtonWithDescriptionAndAuxButton mEnhancedProtectionButton;

    @Before
    public void setUp() {
        SafeBrowsingBridgeJni.setInstanceForTesting(mSafeBrowsingBridgeJni);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = (TestActivity) activity;
    }

    private void launchFragmentWithArgs(Bundle args) {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        mSettings =
                (SafeBrowsingSettingsFragment)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        SafeBrowsingSettingsFragment.class.getClassLoader(),
                                        SafeBrowsingSettingsFragment.class.getName());
        mSettings.setArguments(args);
        mSettings.setProfile(mProfile);

        fragmentManager.beginTransaction().replace(android.R.id.content, mSettings).commit();
        mActivityScenarioRule.getScenario().moveToState(State.STARTED);

        RadioButtonGroupSafeBrowsingPreference safeBrowsingPreference =
                (RadioButtonGroupSafeBrowsingPreference)
                        mSettings.findPreference(SafeBrowsingSettingsFragment.PREF_SAFE_BROWSING);
        mEnhancedProtectionButton =
                (RadioButtonWithDescriptionAndAuxButton)
                        safeBrowsingPreference.getEnhancedProtectionButtonForTesting();
    }

    @Test
    @SmallTest
    public void testEnhancedProtectionHighlight() {
        launchFragmentWithArgs(
                SafeBrowsingSettingsFragment.createArguments(
                        SettingsAccessPoint.TIPS_NOTIFICATIONS_PROMO));

        ColorDrawable initialBackground = (ColorDrawable) mEnhancedProtectionButton.getBackground();
        assertEquals(
                SemanticColorUtils.getSettingsBackgroundColor(mActivity),
                initialBackground.getColor());

        // Run delayed animation that reverts the color.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        ColorDrawable finalBackground = (ColorDrawable) mEnhancedProtectionButton.getBackground();
        assertEquals(SemanticColorUtils.getDefaultBgColor(mActivity), finalBackground.getColor());
    }
}
