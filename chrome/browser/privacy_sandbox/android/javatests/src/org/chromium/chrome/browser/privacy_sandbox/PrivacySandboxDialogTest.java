// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.test.espresso.PerformException;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy_sandbox.v4.AdMeasurementFragmentV4;
import org.chromium.chrome.browser.privacy_sandbox.v4.PrivacySandboxDialogConsentEEAV4;
import org.chromium.chrome.browser.privacy_sandbox.v4.PrivacySandboxDialogNoticeEEAV4;
import org.chromium.chrome.browser.privacy_sandbox.v4.PrivacySandboxDialogNoticeROWV4;
import org.chromium.chrome.browser.privacy_sandbox.v4.PrivacySandboxDialogNoticeRestrictedV4;
import org.chromium.chrome.browser.privacy_sandbox.v4.PrivacySandboxSettingsFragmentV4;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxDialog}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class PrivacySandboxDialogTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public JniMocker mocker = new JniMocker();

    private BottomSheetController mBottomSheetController;

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @Mock
    private SettingsLauncher mSettingsLauncher;

    private Dialog mDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
        mBottomSheetController = sActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        PrivacySandboxDialogController.resetShowNewNoticeForTesting();
        PrivacySandboxDialogController.disableAnimationsForTesting(true);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Dismiss the dialog between the tests. Necessary due to batching.
            if (mDialog != null) {
                mDialog.dismiss();
                mDialog = null;
            }
        });
    }

    private void renderViewWithId(int id, String renderId) {
        onViewWaiting(withId(id));
        onView(withId(id)).check((v, noMatchException) -> {
            if (noMatchException != null) throw noMatchException;
            // Allow disk writes and slow calls to render from UI thread.
            try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
                TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(v));
                mRenderTestRule.render(v, renderId);
            } catch (IOException e) {
                assert false : "Render test failed due to " + e;
            }
        });
    }

    private void launchDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (mDialog != null) {
                mDialog.dismiss();
                mDialog = null;
            }
            PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                    sActivityTestRule.getActivity(), mSettingsLauncher, /*isIncognito=*/false,
                    mBottomSheetController);
            mDialog = PrivacySandboxDialogController.getDialogForTesting();
        });
    }

    private void tryClickOn(Matcher<View> viewMatcher) {
        clickMoreButtonUntilFullyScrolledDown();
        onViewWaiting(viewMatcher).perform(click());
    }

    private void clickMoreButtonUntilFullyScrolledDown() {
        while (true) {
            try {
                onView(withId(R.id.more_button)).perform(click());
                var promptType = mFakePrivacySandboxBridge.getRequiredPromptType();
                if (promptType == PromptType.M1_CONSENT) {
                    assertEquals("Last dialog action", PromptAction.CONSENT_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                } else if (promptType == PromptType.M1_NOTICE_EEA
                        || promptType == PromptType.M1_NOTICE_ROW) {
                    assertEquals("Last dialog action", PromptAction.NOTICE_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                } else if (promptType == PromptType.M1_NOTICE_RESTRICTED) {
                    assertEquals("Last dialog action",
                            PromptAction.RESTRICTED_NOTICE_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                }
            } catch (PerformException e) {
                return;
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderConsent() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialog = new PrivacySandboxDialogConsent(sActivityTestRule.getActivity());
            mDialog.show();
        });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_consent_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderConsentExpanded() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialog = new PrivacySandboxDialogConsent(sActivityTestRule.getActivity());
            mDialog.show();
        });
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        onView(withId(R.id.privacy_sandbox_consent_dropdown)).perform(scrollTo());
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_consent_dialog_expanded");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderNotice() throws IOException {
        PrivacySandboxDialogNotice notice = null;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialog = new PrivacySandboxDialogNotice(
                    sActivityTestRule.getActivity(), mSettingsLauncher);
            mDialog.show();
        });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_notice_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderEEAConsent() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialog = new PrivacySandboxDialogConsentEEAV4(
                    sActivityTestRule.getActivity(), mSettingsLauncher, /*animate = */ false);
            mDialog.show();
        });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_consent_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderEEANotice() throws IOException {
        PrivacySandboxDialogNotice notice = null;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialog = new PrivacySandboxDialogNoticeEEAV4(
                    sActivityTestRule.getActivity(), mSettingsLauncher);
            mDialog.show();
        });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_notice_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderROWNotice() throws IOException {
        PrivacySandboxDialogNotice notice = null;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialog = new PrivacySandboxDialogNoticeROWV4(
                    sActivityTestRule.getActivity(), mSettingsLauncher);
            mDialog.show();
        });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_row_notice_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderRestrictedNotice() throws IOException {
        PrivacySandboxDialogNotice notice = null;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialog = new PrivacySandboxDialogNoticeRestrictedV4(
                    sActivityTestRule.getActivity(), mSettingsLauncher);
            mDialog.show();
        });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_restricted_notice_dialog");
    }

    @Test
    @SmallTest
    public void testControllerIncognito() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                    sActivityTestRule.getActivity(), mSettingsLauncher, /*isIncognito=*/true,
                    mBottomSheetController);
        });
        // Verify that nothing is shown. Notice & Consent share a title.
        onView(withText(R.string.privacy_sandbox_consent_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsNothing() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.NONE);
        launchDialog();
        // Verify that nothing is shown. Notice & Consent share a title.
        onView(withText(R.string.privacy_sandbox_consent_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsConsent() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.CONSENT);
        launchDialog();
        // Verify that the consent is shown and the action is recorded.
        onViewWaiting(withId(R.id.privacy_sandbox_consent_title));
        assertEquals("Last dialog action", PromptAction.CONSENT_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Accept the consent and verify it worked correctly.
        onView(withText(R.string.privacy_sandbox_dialog_yes_button)).perform(click());
        assertEquals("Last dialog action", PromptAction.CONSENT_ACCEPTED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_title)).check(doesNotExist());

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_consent_title));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.CONSENT_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_dropdown)).perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_consent_dropdown)).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.CONSENT_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_dropdown)).check(doesNotExist());

        // Decline the consent and verify it worked correctly.
        onView(withText(R.string.privacy_sandbox_dialog_no_button)).perform(click());
        assertEquals("Last dialog action", PromptAction.CONSENT_DECLINED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testControllerShowsBottomSheet() {
        PrivacySandboxDialogController.setShowNewNoticeForTesting(true);
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.NOTICE);
        launchDialog();
        // Verify that the notice is shown and the action is recorded.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_sheet_title));
        onViewWaiting(withId(R.id.ack_button));
        assertEquals("Last dialog action", PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Acknowledge the notice and verify it worked correctly.
        onView(withText(R.string.privacy_sandbox_dialog_acknowledge_button)).perform(click());
        assertEquals("Last dialog action", PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_sheet_title)).check(doesNotExist());

        launchDialog();
        // Click on the settings button and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_sheet_title));
        onViewWaiting(withId(R.id.settings_button));
        onView(withText(R.string.privacy_sandbox_dialog_settings_button)).perform(click());
        assertEquals("Last dialog action", PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_sheet_title)).check(doesNotExist());
        Context ctx = (Context) sActivityTestRule.getActivity();
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(
                        eq(ctx), eq(PrivacySandboxSettingsFragmentV3.class), any(Bundle.class));
    }

    @Test
    @SmallTest
    public void testControllerWontShowNoticeWhenNewNoticeIsDisabled() {
        PrivacySandboxDialogController.setShowNewNoticeForTesting(false);
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.NOTICE);
        launchDialog();
        onView(withText(R.string.privacy_sandbox_notice_sheet_title)).check(doesNotExist());

        PrivacySandboxDialogController.setShowNewNoticeForTesting(false);
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.NOTICE);
        launchDialog();
        onView(withText(R.string.privacy_sandbox_notice_sheet_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsEEAConsent() throws IOException {
        PrivacySandboxDialogController.disableEEANoticeForTesting(true);

        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Verify that the EEA consent is shown
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title));
        assertEquals("Last dialog action", PromptAction.CONSENT_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Accept the consent and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals("Last dialog action", PromptAction.CONSENT_ACCEPTED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsEEAConsentDropdown() {
        PrivacySandboxDialogController.disableEEANoticeForTesting(true);

        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.CONSENT_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.CONSENT_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(doesNotExist());

        // Decline the consent and verify it worked correctly.
        tryClickOn(withId(R.id.no_button));
        assertEquals("Last dialog action", PromptAction.CONSENT_DECLINED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testAfterEEAConsentSpinnerAndNoticeAreShown() throws IOException {
        PrivacySandboxDialogController.disableAnimationsForTesting(false);

        // Launch the consent
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Accept the consent and verify the spinner it's shown.
        tryClickOn(withId(R.id.ack_button));
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.progress_bar_container)).check(matches(isDisplayed()));

        // Wait for the spinner to disappear and check the notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title)).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_sandbox_m1_consent_title)).check(doesNotExist());
        onView(withId(R.id.progress_bar_container)).check(doesNotExist());

        // Launch the consent
        launchDialog();

        // Decline the consent and verify the spinner it's shown.
        tryClickOn(withId(R.id.no_button));
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.progress_bar_container)).check(matches(isDisplayed()));

        // Wait for the spinner to disappear and check the notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title)).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_sandbox_m1_consent_title)).check(doesNotExist());
        onView(withId(R.id.progress_bar_container)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testControllerShowsEEANotice() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        // Verify that the EEA notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title));
        assertEquals("Last dialog action", PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals("Last dialog action", PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.NOTICE_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown)).perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown)).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.NOTICE_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown)).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        tryClickOn(withId(R.id.settings_button));
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        assertEquals("Last dialog action", PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(any(Context.class),
                        eq(PrivacySandboxSettingsFragmentV4.class), any(Bundle.class));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testControllerShowsROWNotice() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_ROW);
        launchDialog();
        // Verify that the ROW notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title));
        assertEquals("Last dialog action", PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals("Last dialog action", PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.NOTICE_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_row_dropdown)).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", PromptAction.NOTICE_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_row_dropdown)).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        tryClickOn(withId(R.id.settings_button));
        assertEquals("Last dialog action", PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(any(Context.class),
                        eq(PrivacySandboxSettingsFragmentV4.class), any(Bundle.class));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testControllerShowsRestrictedNotice() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_RESTRICTED);
        launchDialog();
        // Verify that the restricted notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title));
        assertEquals("Last dialog action", PromptAction.RESTRICTED_NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals("Last dialog action", PromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        launchDialog();
        tryClickOn(withId(R.id.settings_button));
        assertEquals("Last dialog action", PromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(any(Context.class), eq(AdMeasurementFragmentV4.class));
    }
}
