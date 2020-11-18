// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests {@link VoiceToolbarButtonController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures({ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR + "<FakeStudy",
        "force-fieldtrials=FakeStudy/Enabled"})
public final class VoiceToolbarButtonControllerTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private String mTestPageUrl;
    private String mButtonString;

    @Mock
    VoiceRecognitionHandler mVoiceRecognitionHandler;

    @Before
    public void setUp() {
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        ((LocationBarCoordinator) sActivityTestRule.getActivity()
                        .getToolbarManager()
                        .getToolbarLayoutForTesting()
                        .getLocationBar())
                .setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);

        // Now that we've replaced the handler with a mock, load another page so the button provider
        // is updated (and shown) based on the mocked isVoiceSearchEnabled().
        mTestPageUrl = sActivityTestRule.getTestServer().getURL(TEST_PAGE);
        sActivityTestRule.loadUrl(mTestPageUrl);

        mButtonString = sActivityTestRule.getActivity().getResources().getString(
                R.string.accessibility_toolbar_btn_mic);
    }

    private void assertButtonMissingOrNonVoice() {
        onView(isRoot()).check(
                waitForView(allOf(withId(R.id.optional_toolbar_button), isDisplayed(), isEnabled(),
                                    withContentDescription(mButtonString)),
                        ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=FakeStudy.Enabled:minimum_width_dp/0"})
    public void testVoiceButtonInToolbarIsDisabledOnNTP() {
        // Ensure the button starts visible.
        onView(isRoot()).check(waitForView(allOf(withId(R.id.optional_toolbar_button),
                isDisplayed(), isEnabled(), withContentDescription(mButtonString))));

        sActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        assertButtonMissingOrNonVoice();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=FakeStudy.Enabled:minimum_width_dp/0"})
    public void testVoiceButtonInToolbarIsMissingWhenVoiceDisabled() {
        doReturn(false).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        // Reload the page so the button provider is updated based on the mock.
        sActivityTestRule.loadUrl(mTestPageUrl);

        assertButtonMissingOrNonVoice();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=FakeStudy.Enabled:minimum_width_dp/0"})
    public void testVoiceButtonDisabledOnIncognito() {
        // Ensure the button starts visible.
        onView(isRoot()).check(waitForView(allOf(withId(R.id.optional_toolbar_button),
                isDisplayed(), isEnabled(), withContentDescription(mButtonString))));

        sActivityTestRule.newIncognitoTabFromMenu();

        assertButtonMissingOrNonVoice();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=FakeStudy.Enabled:minimum_width_dp/0"})
    public void testVoiceButtonInToolbarIsDisabledDuringModal() {
        // Ensure the button starts visible.
        onView(isRoot()).check(waitForView(allOf(withId(R.id.optional_toolbar_button),
                isDisplayed(), isEnabled(), withContentDescription(mButtonString))));

        // Get a reference to the button before the modal is opened as it's harder to get after.
        View button = sActivityTestRule.getActivity().findViewById(R.id.optional_toolbar_button);

        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };

        PropertyModel dialogModel = (new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                             .with(ModalDialogProperties.CONTROLLER, controller)
                                             .build());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getModalDialogManager().showDialog(
                    dialogModel, ModalDialogType.APP);
        });

        assertThat(button,
                allOf(isDisplayed(), not(isEnabled()), withContentDescription(mButtonString)));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getModalDialogManager().dismissDialog(
                    dialogModel, DialogDismissalCause.UNKNOWN);
        });
        onView(isRoot()).check(waitForView(allOf(withId(R.id.optional_toolbar_button),
                isDisplayed(), isEnabled(), withContentDescription(mButtonString))));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=FakeStudy.Enabled:minimum_width_dp/0"})
    public void testVoiceButtonInToolbarStartsVoiceRecognition() {
        onViewWaiting(allOf(withId(R.id.optional_toolbar_button), isDisplayed(), isEnabled(),
                              withContentDescription(mButtonString)))
                .perform(click());

        verify(mVoiceRecognitionHandler).startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
    }

    @Test
    @MediumTest
    @RequiresRestart
    @CommandLineFlags.Add({"force-fieldtrial-params=FakeStudy.Enabled:minimum_width_dp/200000"})
    public void testVoiceButtonInToolbarScreenNotWideEnough() {
        assertButtonMissingOrNonVoice();
    }
}
