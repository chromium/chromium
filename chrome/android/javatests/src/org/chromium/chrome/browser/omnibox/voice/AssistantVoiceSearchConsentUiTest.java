// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests for AssistantVoiceSearchConsentDialog */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(wylieb): Batch these tests.
public class AssistantVoiceSearchConsentUiTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    @Mock
    Callback<Boolean> mCallback;

    AssistantVoiceSearchConsentUi mAssistantVoiceSearchConsentUi;
    BottomSheetController mBottomSheetController;
    BottomSheetTestSupport mBottomSheetTestSupport;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mBottomSheetController = cta.getRootUiCoordinatorForTesting().getBottomSheetController();
        mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        mAssistantVoiceSearchConsentUi = new AssistantVoiceSearchConsentUi(cta.getWindowAndroid(),
                cta, mSharedPreferencesManager, new SettingsLauncherImpl(), mBottomSheetController);
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ASSISTANT_VOICE_SEARCH_ENABLED);
    }

    private void showConsentUi() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAssistantVoiceSearchConsentUi.show(mCallback);
            mBottomSheetTestSupport.endAllAnimations();
        });
    }

    @Test
    @MediumTest
    public void testNoBottomSheetControllerAvailable() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AssistantVoiceSearchConsentUi.show(cta.getWindowAndroid(), mSharedPreferencesManager,
                new SettingsLauncherImpl(), null, mCallback);
        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_AcceptButton() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(mAssistantVoiceSearchConsentUi.getContentView().findViewById(
                    R.id.button_primary));
            mBottomSheetTestSupport.endAllAnimations();
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                       ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false),
                    is(true));
        });

        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(true);
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_RejectButton() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(mAssistantVoiceSearchConsentUi.getContentView().findViewById(
                    R.id.button_secondary));
            mBottomSheetTestSupport.endAllAnimations();
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                       ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ true),
                    is(false));
        });

        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Failing on beta in M88 only, crbug.com/1152516")
    public void testDialogInteractivity_LearnMoreButton() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(mAssistantVoiceSearchConsentUi.getContentView().findViewById(
                    R.id.avs_consent_ui_learn_more));
            mBottomSheetTestSupport.endAllAnimations();
        });

        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.avs_setting_category_title)))
                .check(matches(isDisplayed()));
        Mockito.verify(mCallback, Mockito.times(0)).onResult(/* meaningless value */ true);
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_BackButton() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> { mBottomSheetTestSupport.handleBackPress(); });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                       ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ true),
                    is(false));
        });
        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
    }
}