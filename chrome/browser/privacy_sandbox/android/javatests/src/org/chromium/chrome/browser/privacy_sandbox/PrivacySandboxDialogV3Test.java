// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.CoreMatchers.not;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxDialogV3}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class PrivacySandboxDialogV3Test {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_PRIVACY_SANDBOX)
                    .setRevision(2)
                    .setDescription("Changed feature flag behavior for button equalization")
                    .build();

    private PrivacySandboxDialogV3 mDialog;
    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL("/chrome/test/data/android/google.html");
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mDialog != null) {
                        mDialog.dismiss();
                        mDialog = null;
                    }
                });
    }

    private void renderViewWithId(int id, String renderId) {
        onViewWaiting(withId(id), true);
        onView(withId(id))
                .inRoot(isDialog())
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            try {
                                ThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(v));
                                mRenderTestRule.render(v, renderId);
                            } catch (IOException e) {
                                assert false : "Render test failed due to " + e;
                            }
                        });
    }

    private void launchDialog(@PrivacySandboxDialogV3.PrivacySandboxDialogType int dialogType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mDialog != null) {
                        mDialog.dismiss();
                        mDialog = null;
                    }
                    mDialog =
                            new PrivacySandboxDialogV3(
                                    sActivityTestRule.getActivity(),
                                    sActivityTestRule.getProfile(false),
                                    sActivityTestRule.getActivity().getWindowAndroid(),
                                    dialogType);
                    mDialog.show();
                });
        // Wait for the dialog to render.
        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true);
    }

    public void clickMoreButtonAndScrollToBottomIfNeeded() {
        // Repeatedly click on the more button till we hit the bottom of the screen.
        while (mDialog.canScrollVerticallyDown()) {
            onViewWaiting(withId(R.id.more_button), true);
            onView(withId(R.id.bottom_fade)).check(matches(isDisplayed()));
            onView(withId(R.id.more_button)).inRoot(isDialog()).perform(click());
            // TODO(crbug.com/392943234): Assert that a histogram was emitted.
        }
        onView(withId(R.id.more_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.bottom_fade)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderEeaConsent() throws IOException {
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_consent_eea_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderEeaConsentDropdownContent() throws IOException {
        // Expands the dropdown element.
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        onView(withId(R.id.ad_measurement_dropdown_element))
                .inRoot(isDialog())
                .perform(scrollTo(), click());
        renderViewWithId(R.id.privacy_sandbox_dialog, "ad_measurement_dropdown_container");
    }

    @Test
    @SmallTest
    public void testEeaConsentMoreButtonIsNotShown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mDialog != null) {
                        mDialog.dismiss();
                        mDialog = null;
                    }
                    mDialog =
                            new PrivacySandboxDialogV3(
                                    sActivityTestRule.getActivity(),
                                    sActivityTestRule.getProfile(false),
                                    sActivityTestRule.getActivity().getWindowAndroid(),
                                    PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
                    // Resize the window such that we see the entire notice without scrolling.
                    // Note that we're picking an arbitrary height value that should capture all the
                    // content.
                    mDialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, 50000);
                    mDialog.show();
                });
        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true);
        // Verify the more button and the fade are not shown.
        onView(withId(R.id.more_button)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.bottom_fade)).check(matches(withEffectiveVisibility(GONE)));
        // Verify that the action buttons are shown.
        onView(withId(R.id.no_button)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.ack_button)).check(matches(withEffectiveVisibility(VISIBLE)));
    }

    @Test
    @SmallTest
    public void testEeaConsentActionButtonsAreShown() {
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        clickMoreButtonAndScrollToBottomIfNeeded();
        // Verify action buttons are shown
        onView(withId(R.id.no_button)).check(matches(isDisplayed()));
        onView(withId(R.id.ack_button)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testEeaConsentAcceptButtonDismissesDialog() {
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        onView(withId(R.id.privacy_sandbox_consent_title)).check(matches(isDisplayed()));
        clickMoreButtonAndScrollToBottomIfNeeded();
        onViewWaiting(withId(R.id.ack_button), true);
        onView(withId(R.id.ack_button)).inRoot(isDialog()).perform(click());
        onView(withId(R.id.privacy_sandbox_consent_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testEeaConsentDeclineButtonDismissesDialog() {
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        onView(withId(R.id.privacy_sandbox_consent_title)).check(matches(isDisplayed()));
        clickMoreButtonAndScrollToBottomIfNeeded();
        onViewWaiting(withId(R.id.no_button), true);
        onView(withId(R.id.no_button)).inRoot(isDialog()).perform(click());
        onView(withId(R.id.privacy_sandbox_consent_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testEeaConsentActionButtonsAreSticky() {
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        clickMoreButtonAndScrollToBottomIfNeeded();
        // Verify action buttons are shown
        onView(withId(R.id.no_button)).check(matches(isDisplayed()));
        onView(withId(R.id.ack_button)).check(matches(isDisplayed()));
        // Scroll back to the top
        onView(withId(R.id.privacy_sandbox_consent_title)).inRoot(isDialog()).perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_consent_title)).check(matches(isDisplayed()));
        // Verify the more button and fade are not displayed.
        onView(withId(R.id.more_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.bottom_fade)).check(matches(not(isDisplayed())));
        // Verify action buttons are shown
        onView(withId(R.id.no_button)).check(matches(isDisplayed()));
        onView(withId(R.id.ack_button)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testEEAConsentPrivacyPolicyLink() {
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        onView(withId(R.id.learn_more_text)).inRoot(isDialog()).perform(scrollTo(), click());
        // Validate Privacy Policy View is not shown
        onView(withId(R.id.privacy_policy_view))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
        // Click "Privacy Policy" link
        onView(withId(R.id.learn_more_text)).inRoot(isDialog()).perform(clickOnClickableSpan(0));
        // TODO(crbug.com/392943234): Assert that a histogram was emitted when the link was
        // clicked.
        // Validate EEA Consent is not shown
        onView(withId(R.id.privacy_sandbox_consent_eea_view))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
        // Validate Privacy Policy View is shown
        onView(withId(R.id.privacy_policy_view)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_policy_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_policy_back_button))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        // Click back button
        onView(withId(R.id.privacy_policy_back_button)).inRoot(isDialog()).perform(click());
        // TODO(crbug.com/392943234): Assert that a histogram was emitted when the back button
        // was clicked.
        // Validate EEA Consent is shown
        onView(withId(R.id.privacy_sandbox_consent_eea_view))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        // Validate Privacy Policy View is not shown
        onView(withId(R.id.privacy_policy_view))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testEEAConsentDropdown() {
        launchDialog(PrivacySandboxDialogV3.PrivacySandboxDialogType.EEA_CONSENT);
        onView(withId(R.id.ad_measurement_dropdown_element)).inRoot(isDialog()).perform(scrollTo());
        // Validate dropdown content is not shown
        onView(withId(R.id.ad_measurement_dropdown_container))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
        // Expand the dropdown element.
        onView(withId(R.id.ad_measurement_dropdown_element))
                .inRoot(isDialog())
                .perform(scrollTo(), click());
        // Validate the dropdown content is shown.
        onView(withId(R.id.ad_measurement_dropdown_container))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        // We need to scroll to the top separator so that the dropdown element is clickable, if we
        // scroll to the dropdown element it becomes unclickable - blocked by the android status bar
        // at the top.
        onView(withId(R.id.top_separator_for_dropdown)).inRoot(isDialog()).perform(scrollTo());
        // Retract the dropdown element.
        onView(withId(R.id.ad_measurement_dropdown_element)).inRoot(isDialog()).perform(click());
        // Validate the dropdown content is not shown
        onView(withId(R.id.ad_measurement_dropdown_container))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }
}
