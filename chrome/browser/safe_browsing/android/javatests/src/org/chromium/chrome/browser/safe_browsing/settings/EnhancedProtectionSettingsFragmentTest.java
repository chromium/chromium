// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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

    private void launchSettingsActivity() {
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

    // TODO(crbug.com/1478337): Add a test to check the openUrlInCCT functionality.

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    public void testFriendlierSafeBrowsingSettingsEnhancedProtection() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
                });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Check that the learn more label is shown
                    Assert.assertNotNull(mEnhancedProtectionLearnMore);

                    EnhancedProtectionSettingsFragment fragment = mTestRule.getFragment();

                    String enhancedProtectionSubtitle =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_subtitle_updated);
                    String whenOn = fragment.getContext().getString(R.string.privacy_guide_when_on);
                    String friendlierBulletOne =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_one_updated);
                    String friendlierBulletTwo =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_two_updated);
                    String friendlierBulletThree =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_three_updated);
                    String friendlierBulletFour =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_four_updated);
                    String friendlierBulletFive =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_five_updated);
                    String thingsToConsider =
                            fragment.getContext()
                                    .getString(R.string.privacy_guide_things_to_consider);
                    String friendlierBulletSix =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_six_updated);
                    String friendlierBulletSeven =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_seven_updated);
                    String friendlierBulletEight =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_bullet_eight_updated);

                    Assert.assertEquals(
                            enhancedProtectionSubtitle, mEnhancedProtectionSubtitle.getTitle());
                    Assert.assertEquals(whenOn, mEnhancedProtectionWhenOn.getTitle());
                    Assert.assertEquals(
                            friendlierBulletOne, mEnhancedProtectionBulletOne.getSummary());
                    Assert.assertEquals(
                            friendlierBulletTwo, mEnhancedProtectionBulletTwo.getSummary());
                    Assert.assertEquals(
                            friendlierBulletThree, mEnhancedProtectionBulletThree.getSummary());
                    Assert.assertEquals(
                            friendlierBulletFour, mEnhancedProtectionBulletFour.getSummary());
                    Assert.assertEquals(
                            friendlierBulletFive, mEnhancedProtectionBulletFive.getSummary());
                    Assert.assertEquals(
                            thingsToConsider, mEnhancedProtectionThingsToConsider.getTitle());
                    Assert.assertEquals(
                            friendlierBulletSix, mEnhancedProtectionBulletSix.getSummary());
                    Assert.assertEquals(
                            friendlierBulletSeven, mEnhancedProtectionBulletSeven.getSummary());
                    Assert.assertEquals(
                            friendlierBulletEight, mEnhancedProtectionBulletEight.getSummary());
                });
    }

    @Test
    @SmallTest
    @Feature({"SafeBrowsing"})
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    public void testDisabledFriendlierSafeBrowsingSettingsEnhancedProtection() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
                });
        launchSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Check that the extra bullet points and friendlier When On/Things to Consider
                    // headings and the learn more label are gone.
                    Assert.assertNull(mEnhancedProtectionWhenOn);
                    Assert.assertNull(mEnhancedProtectionThingsToConsider);
                    Assert.assertNull(mEnhancedProtectionBulletSix);
                    Assert.assertNull(mEnhancedProtectionBulletSeven);
                    Assert.assertNull(mEnhancedProtectionBulletEight);
                    Assert.assertNull(mEnhancedProtectionLearnMore);

                    EnhancedProtectionSettingsFragment fragment = mTestRule.getFragment();

                    String enhancedProtectionSubtitle =
                            fragment.getContext()
                                    .getString(R.string.safe_browsing_enhanced_protection_subtitle);
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

                    Assert.assertEquals(
                            enhancedProtectionSubtitle, mEnhancedProtectionSubtitle.getTitle());
                    Assert.assertEquals(bulletOne, mEnhancedProtectionBulletOne.getSummary());
                    Assert.assertEquals(bulletTwo, mEnhancedProtectionBulletTwo.getSummary());
                    Assert.assertEquals(bulletThree, mEnhancedProtectionBulletThree.getSummary());
                    Assert.assertEquals(bulletFour, mEnhancedProtectionBulletFour.getSummary());
                    Assert.assertEquals(bulletFive, mEnhancedProtectionBulletFive.getSummary());
                });
    }
}
