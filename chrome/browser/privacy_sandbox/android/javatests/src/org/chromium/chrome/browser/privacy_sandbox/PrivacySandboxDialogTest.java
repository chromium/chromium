// Copyright 2022 The Chromium Authors. All rights reserved.
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

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxDialog}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
public final class PrivacySandboxDialogTest {
    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

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

    @BeforeClass
    public static void beforeClass() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
        mBottomSheetController = sActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
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
    @DisabledTest(message = "https://crbug.com/1300632")
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
        assertEquals("Last dialog action", DialogAction.CONSENT_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        // Accept the consent and verify it worked correctly.
        onView(withText(R.string.privacy_sandbox_dialog_yes_button)).perform(click());
        assertEquals("Last dialog action", DialogAction.CONSENT_ACCEPTED,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        onView(withId(R.id.privacy_sandbox_consent_title)).check(doesNotExist());

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_consent_title));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", DialogAction.CONSENT_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        onView(withId(R.id.privacy_sandbox_consent_dropdown)).perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_consent_dropdown)).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).perform(scrollTo(), click());
        assertEquals("Last dialog action", DialogAction.CONSENT_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        onView(withId(R.id.privacy_sandbox_consent_dropdown)).check(doesNotExist());

        // Decline the consent and verify it worked correctly.
        onView(withText(R.string.privacy_sandbox_dialog_no_button)).perform(click());
        assertEquals("Last dialog action", DialogAction.CONSENT_DECLINED,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        onView(withId(R.id.privacy_sandbox_consent_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsNotice() throws IOException, InterruptedException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.NOTICE);
        launchDialog();
        // Verify that the consent is shown and the action is recorded.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title));
        assertEquals("Last dialog action", DialogAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        // Acknowledge the notice and verify it worked correctly.
        onView(withText(R.string.privacy_sandbox_dialog_acknowledge_button)).perform(click());
        assertEquals("Last dialog action", DialogAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());

        launchDialog();
        // Click on the settings button and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title));
        onView(withText(R.string.privacy_sandbox_dialog_settings_button)).perform(click());
        assertEquals("Last dialog action", DialogAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastDialogAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Context ctx = (Context) sActivityTestRule.getActivity();
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(
                        eq(ctx), eq(PrivacySandboxSettingsFragmentV3.class), any(Bundle.class));
    }
}
