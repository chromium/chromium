// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefetch.settings;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.settings.SettingsFeatureList;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link PreloadPagesSettingsFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test launches a Settings activity")
public class PreloadPagesSettingsFragmentTest {
    private static final String ASSERT_PRELOAD_PAGES_STATE_RADIO_BUTTON_GROUP =
            "Incorrect Preload Pages state in the radio button group.";
    private static final String ASSERT_RADIO_BUTTON_CHECKED =
            "Incorrect radio button checked state.";
    private static final String ASSERT_PRELOAD_PAGES_STATE_NATIVE =
            "Incorrect Preload Pages state from native.";

    @Rule
    public SettingsActivityTestRule<PreloadPagesSettingsFragment> mTestRule =
            new SettingsActivityTestRule<>(PreloadPagesSettingsFragment.class);

    @Mock
    private SettingsLauncher mSettingsLauncher;

    @Mock
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private PreloadPagesSettingsFragment mPreloadPagesSettingsFragment;
    private RadioButtonGroupPreloadPagesSettings mPreloadPagesPreference;
    private TextMessagePreference mManagedTextPreferenceLegacy;
    private Preference mManagedDisclaimerText;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private void launchSettingsActivity() {
        mTestRule.startSettingsActivity();
        mPreloadPagesSettingsFragment = mTestRule.getFragment();
        mPreloadPagesPreference = mPreloadPagesSettingsFragment.findPreference(
                PreloadPagesSettingsFragment.PREF_PRELOAD_PAGES);
        mManagedTextPreferenceLegacy = mPreloadPagesSettingsFragment.findPreference(
                PreloadPagesSettingsFragment.PREF_TEXT_MANAGED_LEGACY);
        mManagedDisclaimerText = mPreloadPagesSettingsFragment.findPreference(
                PreloadPagesSettingsFragment.PREF_MANAGED_DISCLAIMER_TEXT);
        Assert.assertNotNull(
                "Preload Pages preference should not be null.", mPreloadPagesPreference);
        Assert.assertNotNull(
                "Legacy text managed preference should not be null.", mManagedTextPreferenceLegacy);
        Assert.assertNotNull(
                "Managed disclaimer text preference should not be null.", mManagedDisclaimerText);
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    @Features.EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testOnStartup_EnableHighlight() {
        testOnStartupImpl();
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    @Features.DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testOnStartup_DisableHighlight() {
        testOnStartupImpl();
    }

    private void testOnStartupImpl() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            @PreloadPagesState
            int currentState = PreloadPagesSettingsBridge.getState();
            boolean extended_preloading_checked =
                    currentState == PreloadPagesState.EXTENDED_PRELOADING;
            boolean standard_preloading_checked =
                    currentState == PreloadPagesState.STANDARD_PRELOADING;
            boolean no_preloading_checked = currentState == PreloadPagesState.NO_PRELOADING;
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, extended_preloading_checked,
                    getExtendedPreloadingButton().isChecked());
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, standard_preloading_checked,
                    getStandardPreloadingButton().isChecked());
            Assert.assertEquals(ASSERT_RADIO_BUTTON_CHECKED, no_preloading_checked,
                    getNoPreloadingButton().isChecked());
            Assert.assertFalse(mManagedTextPreferenceLegacy.isVisible());
            Assert.assertFalse(mManagedDisclaimerText.isVisible());
        });
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    @Features.EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testCheckRadioButtons_EnableHighlight() {
        testCheckRadioButtonsImpl();
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    @Features.DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testCheckRadioButtons_DisableHighlight() {
        testCheckRadioButtonsImpl();
    }

    public void testCheckRadioButtonsImpl() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mManagedTextPreferenceLegacy.isVisible());
            Assert.assertFalse(mManagedDisclaimerText.isVisible());
            // Click the Extended Preloading button.
            getExtendedPreloadingButton().onClick(null);
            Assert.assertEquals(ASSERT_PRELOAD_PAGES_STATE_RADIO_BUTTON_GROUP,
                    PreloadPagesState.EXTENDED_PRELOADING, getPreloadPagesState());
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getExtendedPreloadingButton().isChecked());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardPreloadingButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoPreloadingButton().isChecked());
            Assert.assertEquals(ASSERT_PRELOAD_PAGES_STATE_NATIVE,
                    PreloadPagesState.EXTENDED_PRELOADING, PreloadPagesSettingsBridge.getState());

            // Click the Standard Preloading button.
            getStandardPreloadingButton().onClick(null);
            Assert.assertEquals(ASSERT_PRELOAD_PAGES_STATE_RADIO_BUTTON_GROUP,
                    PreloadPagesState.STANDARD_PRELOADING, getPreloadPagesState());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getExtendedPreloadingButton().isChecked());
            Assert.assertTrue(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardPreloadingButton().isChecked());
            Assert.assertFalse(ASSERT_RADIO_BUTTON_CHECKED, getNoPreloadingButton().isChecked());
            Assert.assertEquals(ASSERT_PRELOAD_PAGES_STATE_NATIVE,
                    PreloadPagesState.STANDARD_PRELOADING, PreloadPagesSettingsBridge.getState());

            // Click the No Preloading button.
            getNoPreloadingButton().onClick(null);
            Assert.assertEquals(ASSERT_PRELOAD_PAGES_STATE_RADIO_BUTTON_GROUP,
                    PreloadPagesState.NO_PRELOADING, getPreloadPagesState());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getExtendedPreloadingButton().isChecked());
            Assert.assertFalse(
                    ASSERT_RADIO_BUTTON_CHECKED, getStandardPreloadingButton().isChecked());
            Assert.assertTrue(ASSERT_RADIO_BUTTON_CHECKED, getNoPreloadingButton().isChecked());
            Assert.assertEquals(ASSERT_PRELOAD_PAGES_STATE_NATIVE, PreloadPagesState.NO_PRELOADING,
                    PreloadPagesSettingsBridge.getState());
        });
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    public void testExtendedPreloadingAuxButtonClicked() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPreloadPagesSettingsFragment.setSettingsLauncher(mSettingsLauncher);
            getExtendedPreloadingButton().getAuxButtonForTests().performClick();
            Mockito.verify(mSettingsLauncher)
                    .launchSettingsActivity(mPreloadPagesSettingsFragment.getContext(),
                            ExtendedPreloadingSettingsFragment.class);
        });
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    public void testStandardPreloadingAuxButtonClicked() {
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPreloadPagesSettingsFragment.setSettingsLauncher(mSettingsLauncher);
            getStandardPreloadingButton().getAuxButtonForTests().performClick();
            Mockito.verify(mSettingsLauncher)
                    .launchSettingsActivity(mPreloadPagesSettingsFragment.getContext(),
                            StandardPreloadingSettingsFragment.class);
        });
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    @Features.EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @Policies.Add({
        @Policies.Item(key = "NetworkPredictionOptions",
                string = "2" /* NetworkPredictionOptions::kDisabled */)
    })
    public void
    testPreloadingManaged_EnableHighlight() {
        testPreloadingManagedImpl();
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    @Features.DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @Policies.Add({
        @Policies.Item(key = "NetworkPredictionOptions",
                string = "2" /* NetworkPredictionOptions::kDisabled */)
    })
    public void
    testPreloadingManaged_DisableHighlight() {
        testPreloadingManagedImpl();
    }

    public void testPreloadingManagedImpl() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });
        launchSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(PreloadPagesSettingsBridge.isNetworkPredictionManaged());
            if (SettingsFeatureList.isEnabled(
                        SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)) {
                Assert.assertFalse(mManagedTextPreferenceLegacy.isVisible());
                Assert.assertTrue(mManagedDisclaimerText.isVisible());
            } else {
                Assert.assertTrue(mManagedTextPreferenceLegacy.isVisible());
                Assert.assertFalse(mManagedDisclaimerText.isVisible());
            }
            Assert.assertFalse(getExtendedPreloadingButton().isEnabled());
            Assert.assertFalse(getStandardPreloadingButton().isEnabled());
            Assert.assertFalse(getNoPreloadingButton().isEnabled());
            Assert.assertEquals(PreloadPagesState.NO_PRELOADING, getPreloadPagesState());
            // To disclose information, aux buttons should be enabled under managed mode.
            Assert.assertTrue(getExtendedPreloadingButton().getAuxButtonForTests().isEnabled());
            Assert.assertTrue(getStandardPreloadingButton().getAuxButtonForTests().isEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"PreloadPages"})
    public void testHelpButtonClicked() {
        launchSettingsActivity();
        mPreloadPagesSettingsFragment.setHelpAndFeedbackLauncher(mHelpAndFeedbackLauncher);
        onView(withId(R.id.menu_id_targeted_help)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Mockito.verify(mHelpAndFeedbackLauncher)
                    .show(mPreloadPagesSettingsFragment.getActivity(),
                            mPreloadPagesSettingsFragment.getString(R.string.help_context_privacy),
                            null);
        });
    }

    private @PreloadPagesState int getPreloadPagesState() {
        return mPreloadPagesPreference.getPreloadPagesStateForTesting();
    }

    private RadioButtonWithDescriptionAndAuxButton getExtendedPreloadingButton() {
        return mPreloadPagesPreference.getExtendedPreloadingButtonForTesting();
    }

    private RadioButtonWithDescriptionAndAuxButton getStandardPreloadingButton() {
        return mPreloadPagesPreference.getStandardPreloadingButtonForTesting();
    }

    private RadioButtonWithDescription getNoPreloadingButton() {
        return mPreloadPagesPreference.getNoPreloadingButtonForTesting();
    }
}
