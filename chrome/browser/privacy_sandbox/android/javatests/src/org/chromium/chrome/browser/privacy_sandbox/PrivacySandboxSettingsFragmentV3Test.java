// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
public final class PrivacySandboxSettingsFragmentV3Test {
    private static final String REFERRER_HISTOGRAM =
            "Settings.PrivacySandbox.PrivacySandboxReferrer";
    @Rule
    public SettingsActivityTestRule<PrivacySandboxSettingsFragmentV3> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySandboxSettingsFragmentV3.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @Rule
    public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @BeforeClass
    public static void beforeClass() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
    }

    private void openPrivacySandboxSettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.PRIVACY_SETTINGS);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
    }

    private View getRootView(@StringRes int text) {
        View[] view = {null};
        onView(withText(text)).check(((v, e) -> view[0] = v.getRootView()));
        TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderMainPage() throws IOException {
        openPrivacySandboxSettings();
        mRenderTestRule.render(
                getRootView(R.string.privacy_sandbox_trials_title), "privacy_sandbox_main_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderAdPersonalizationView() throws IOException {
        mFakePrivacySandboxBridge.setCurrentTopTopics("Generated sample data", "More made up data");
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        mRenderTestRule.render(getRootView(R.string.privacy_sandbox_topic_interests_category),
                "privacy_sandbox_ad_personalization_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testRenderRemovedInterestsView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_remove_interest_title)).perform(click());
        mRenderTestRule.render(getRootView(R.string.privacy_sandbox_topic_interests_category),
                "privacy_sandbox_removed_interests_view");
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testMainSettingsView() throws IOException {
        openPrivacySandboxSettings();
        assertTrue(PrivacySandboxBridge.isPrivacySandboxEnabled());
        // Toggle sandbox settings.
        onView(withText(R.string.privacy_sandbox_trials_title)).perform(click());
        assertFalse(PrivacySandboxBridge.isPrivacySandboxEnabled());
        onView(withText(R.string.privacy_sandbox_trials_title)).perform(click());
        assertTrue(PrivacySandboxBridge.isPrivacySandboxEnabled());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testAdPersonalizationView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        // Click on the image_button of a the preference with "Foo" text.
        onView(allOf(withId(R.id.image_button), withParent(hasSibling(withChild(withText("Foo"))))))
                .perform(click());
        assertThat(PrivacySandboxBridge.getCurrentTopTopics(), not(hasItem("Foo")));
        assertThat(PrivacySandboxBridge.getBlockedTopics(), hasItem("Foo"));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testRemovedInterestsView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        onView(withText(R.string.privacy_sandbox_remove_interest_title)).perform(click());
        // Click on the image_button of a the preference with "BlockedFoo" text.
        onView(allOf(withId(R.id.image_button),
                       withParent(hasSibling(withChild(withText("BlockedFoo"))))))
                .perform(click());
        assertThat(PrivacySandboxBridge.getCurrentTopTopics(), hasItem("BlockedFoo"));
        assertThat(PrivacySandboxBridge.getBlockedTopics(), not(hasItem("BlockedFoo")));
    }

    @Test
    @SmallTest
    public void testCreateActivityFromPrivacySettings() {
        openPrivacySandboxSettings();
        assertEquals("Total histogram count wrong", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Privacy referrer histogram count", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.PRIVACY_SETTINGS));
    }

    @Test
    @SmallTest
    public void testCreateActivityFromCookiesSnackbar() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.COOKIES_SNACKBAR);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);

        assertEquals("Total histogram count", 1,
                mHistogramTestRule.getHistogramTotalCount(REFERRER_HISTOGRAM));
        assertEquals("Cookies snackbar referrer histogram count wrong", 1,
                mHistogramTestRule.getHistogramValueCount(
                        REFERRER_HISTOGRAM, PrivacySandboxReferrer.COOKIES_SNACKBAR));
    }
}
