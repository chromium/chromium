// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.ViewUtils;

/** Tests {@link VoiceToolbarButtonController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
public final class VoiceToolbarButtonControllerTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private String mTestPageUrl;
    private String mButtonString;

    @Mock VoiceRecognitionHandler mVoiceRecognitionHandler;
    private WebPageStation mPage;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(AdaptiveToolbarButtonVariant.VOICE);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(null);
    }

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        ((LocationBarCoordinator)
                        mActivityTestRule
                                .getActivity()
                                .getToolbarManager()
                                .getToolbarLayoutForTesting()
                                .getLocationBar())
                .setVoiceRecognitionHandlerForTesting(mVoiceRecognitionHandler);

        // Now that we've replaced the handler with a mock, load another page so the button provider
        // is updated (and shown) based on the mocked isVoiceSearchEnabled().
        mTestPageUrl = mActivityTestRule.getTestServer().getURL(TEST_PAGE);
        mPage = mPage.loadWebPageProgrammatically(mTestPageUrl);

        mButtonString =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.accessibility_toolbar_btn_mic);
    }

    private void assertButtonMissingOrNonVoice() {
        ViewUtils.waitForViewCheckingState(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        isEnabled(),
                        withContentDescription(mButtonString)),
                ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL);
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testVoiceButtonInToolbarIsDisabledOnNtp() {
        // Ensure the button starts visible.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        isEnabled(),
                        withContentDescription(mButtonString)));

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        assertButtonMissingOrNonVoice();
    }

    @Test
    @MediumTest
    public void testVoiceButtonInToolbarIsMissingWhenVoiceDisabled() {
        doReturn(false).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        // Reload the page so the button provider is updated based on the mock.
        mActivityTestRule.loadUrl(mTestPageUrl);

        assertButtonMissingOrNonVoice();
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testVoiceButtonDisabledOnIncognito() {
        // Ensure the button starts visible.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        isEnabled(),
                        withContentDescription(mButtonString)));

        mPage.openNewIncognitoTabOrWindowFast();

        assertButtonMissingOrNonVoice();
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testVoiceButtonInToolbarIsDisabledDuringModal() {
        // Ensure the button starts visible.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        isEnabled(),
                        withContentDescription(mButtonString)));

        // Get a reference to the button before the modal is opened as it's harder to get after.
        View button = mActivityTestRule.getActivity().findViewById(R.id.optional_toolbar_button);

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {}

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {}
                };

        PropertyModel dialogModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                    .with(ModalDialogProperties.CONTROLLER, controller)
                                    .build();
                        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getModalDialogManager()
                            .showDialog(dialogModel, ModalDialogType.APP);
                });

        assertThat(
                button,
                allOf(isDisplayed(), not(isEnabled()), withContentDescription(mButtonString)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getModalDialogManager()
                            .dismissDialog(dialogModel, DialogDismissalCause.UNKNOWN);
                });
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        isEnabled(),
                        withContentDescription(mButtonString)));
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testVoiceButtonInToolbarStartsVoiceRecognition() {
        onViewWaiting(
                        allOf(
                                withId(R.id.optional_toolbar_button),
                                isDisplayed(),
                                isEnabled(),
                                withContentDescription(mButtonString)))
                .perform(click());

        verify(mVoiceRecognitionHandler).startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
    }
}
