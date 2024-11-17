// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.TextMessagePreference;

/** Tests for {@link EnhancedProtectionSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class EnhancedProtectionSettingsFragmentTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public SettingsActivityTestRule<EnhancedProtectionSettingsFragment> mTestRule =
            new SettingsActivityTestRule<>(EnhancedProtectionSettingsFragment.class);

    private TextMessagePreference mEnhancedProtectionSubtitle;
    private TextMessagePreference mEnhancedProtectionWhenOn;
    private TextMessagePreference mEnhancedProtectionBulletOne;
    private TextMessagePreference mEnhancedProtectionBulletTwo;
    private TextMessagePreference mEnhancedProtectionBulletThree;
    private TextMessagePreference mEnhancedProtectionBulletFour;
    private TextMessagePreference mEnhancedProtectionBulletFive;
    private TextMessagePreference mEnhancedProtectionThingsToConsider;
    private TextMessagePreference mEnhancedProtectionBulletSix;
    private TextMessagePreference mEnhancedProtectionBulletSeven;
    private TextMessagePreference mEnhancedProtectionBulletEight;
    private TextMessagePreference mEnhancedProtectionLearnMore;

    private static final String PREF_SUBTITLE = "subtitle";
    private static final String PREF_WHENON = "when_on";
    private static final String PREF_BULLETONE = "bullet_one";
    private static final String PREF_BULLETTWO = "bullet_two";
    private static final String PREF_BULLETTHREE = "bullet_three";
    private static final String PREF_BULLETFOUR = "bullet_four";
    private static final String PREF_BULLETFIVE = "bullet_five";
    private static final String PREF_THINGSTOCONSIDER = "things_to_consider";
    private static final String PREF_BULLETSIX = "bullet_six";
    private static final String PREF_BULLETSEVEN = "bullet_seven";
    private static final String PREF_BULLETEIGHT = "bullet_eight";

    private void startSettings() {
        mTestRule.startSettingsActivity();
        EnhancedProtectionSettingsFragment fragment = mTestRule.getFragment();
        mEnhancedProtectionSubtitle = fragment.findPreference(PREF_SUBTITLE);
        mEnhancedProtectionWhenOn = fragment.findPreference(PREF_WHENON);
        mEnhancedProtectionBulletOne = fragment.findPreference(PREF_BULLETONE);
        mEnhancedProtectionBulletTwo = fragment.findPreference(PREF_BULLETTWO);
        mEnhancedProtectionBulletThree = fragment.findPreference(PREF_BULLETTHREE);
        mEnhancedProtectionBulletFour = fragment.findPreference(PREF_BULLETFOUR);
        mEnhancedProtectionBulletFive = fragment.findPreference(PREF_BULLETFIVE);
        mEnhancedProtectionThingsToConsider = fragment.findPreference(PREF_THINGSTOCONSIDER);
        mEnhancedProtectionBulletSix = fragment.findPreference(PREF_BULLETSIX);
        mEnhancedProtectionBulletSeven = fragment.findPreference(PREF_BULLETSEVEN);
        mEnhancedProtectionBulletEight = fragment.findPreference(PREF_BULLETEIGHT);
        mEnhancedProtectionLearnMore =
                fragment.findPreference(EnhancedProtectionSettingsFragment.PREF_LEARN_MORE);
    }

    // TODO(crbug.com/40929404): Add a test to check the openUrlInCCT functionality.

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    public void testSafeBrowsingSettingsEnhancedProtection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new SafeBrowsingBridge(ProfileManager.getLastUsedRegularProfile())
                            .setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
                });
        startSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Check that the learn more label is shown
                    Assert.assertNotNull(mEnhancedProtectionLearnMore);

                    EnhancedProtectionSettingsFragment fragment = mTestRule.getFragment();

                    String enhancedProtectionSubtitle =
                            fragment.getContext()
                                    .getString(R.string.safe_browsing_enhanced_protection_subtitle);
                    String whenOn = fragment.getContext().getString(R.string.privacy_guide_when_on);
                    String bulletOne =
                            fragment.getContext()
                                    .getString(
                                            R.string.safe_browsing_enhanced_protection_bullet_one);
                    String bulletTwo =
                            fragment.getContext()
                                    .getString(
                                            R.string.safe_browsing_enhanced_protection_bullet_two);
                    String bulletThree =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_three);
                    String bulletFour =
                            fragment.getContext()
                                    .getString(
                                            R.string.safe_browsing_enhanced_protection_bullet_four);
                    String bulletFive =
                            fragment.getContext()
                                    .getString(
                                            R.string.safe_browsing_enhanced_protection_bullet_five);
                    String thingsToConsider =
                            fragment.getContext()
                                    .getString(R.string.privacy_guide_things_to_consider);
                    String bulletSix =
                            fragment.getContext()
                                    .getString(
                                            R.string.safe_browsing_enhanced_protection_bullet_six);
                    String bulletSeven =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_seven);
                    String bulletEight =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_eight);

                    Assert.assertEquals(
                            enhancedProtectionSubtitle, mEnhancedProtectionSubtitle.getTitle());
                    Assert.assertEquals(whenOn, mEnhancedProtectionWhenOn.getTitle());
                    Assert.assertEquals(bulletOne, mEnhancedProtectionBulletOne.getSummary());
                    Assert.assertEquals(bulletTwo, mEnhancedProtectionBulletTwo.getSummary());
                    Assert.assertEquals(bulletThree, mEnhancedProtectionBulletThree.getSummary());
                    Assert.assertEquals(bulletFour, mEnhancedProtectionBulletFour.getSummary());
                    Assert.assertEquals(bulletFive, mEnhancedProtectionBulletFive.getSummary());
                    Assert.assertEquals(
                            thingsToConsider, mEnhancedProtectionThingsToConsider.getTitle());
                    Assert.assertEquals(bulletSix, mEnhancedProtectionBulletSix.getSummary());
                    Assert.assertEquals(bulletSeven, mEnhancedProtectionBulletSeven.getSummary());
                    Assert.assertEquals(bulletEight, mEnhancedProtectionBulletEight.getSummary());
                });
    }
}
