// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.runner.lifecycle.Stage;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.chrome.browser.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.chrome.browser.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests {@link BottomSheetOnboardingCoordinator}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class BottomSheetOnboardingCoordinatorTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Callback<Integer> mCallback;

    private ChromeActivity mActivity;
    private BottomSheetController mBottomSheetController;
    private ScrimCoordinator mScrimCoordinator;
    private OnboardingCoordinatorFactory mOnboardingCoordinatorFactory;

    @Before
    public void setUp() throws Exception {
        AutofillAssistantUiTestUtil.startOnBlankPage(mCustomTabActivityTestRule);
        mActivity = mCustomTabActivityTestRule.getActivity();
        mBottomSheetController = TestThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantUiTestUtil.getBottomSheetController(mActivity));
        mScrimCoordinator = mCustomTabActivityTestRule.getActivity()
                                    .getRootUiCoordinatorForTesting()
                                    .getScrimCoordinator();
        mOnboardingCoordinatorFactory =
                new OnboardingCoordinatorFactory(mActivity, mBottomSheetController,
                        mActivity.getBrowserControlsManager(), mActivity.getCompositorViewHolder());
    }

    private BaseOnboardingCoordinator createCoordinator() {
        return createCoordinator("", new HashMap<>());
    }

    private BaseOnboardingCoordinator createCoordinator(
            String experimentIds, Map<String, String> parameters) {
        BaseOnboardingCoordinator coordinator =
                mOnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(
                        experimentIds, parameters);
        coordinator.disableAnimationForTesting();
        return coordinator;
    }

    @Test
    @MediumTest
    public void testAcceptOnboarding() throws Exception {
        testOnboarding(R.id.button_init_ok, AssistantOnboardingResult.ACCEPTED);
    }

    @Test
    @MediumTest
    public void testRejectOnboarding() throws Exception {
        testOnboarding(R.id.button_init_not_ok, AssistantOnboardingResult.REJECTED);
    }

    private void testOnboarding(@IdRes int buttonToClick,
            @AssistantOnboardingResult int expectedResult) throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(
                expectedResult != AssistantOnboardingResult.ACCEPTED);

        BaseOnboardingCoordinator coordinator = createCoordinator();
        showOnboardingAndWait(coordinator, mCallback);

        assertTrue(TestThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));
        onView(is(mScrimCoordinator.getViewForTesting())).check(matches(isDisplayed()));
        onView(withId(buttonToClick)).perform(scrollTo(), click());

        verify(mCallback).onResult(expectedResult);
        assertFalse(TestThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));
        assertEquals(expectedResult == AssistantOnboardingResult.ACCEPTED,
                AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted());
    }

    @Test
    @MediumTest
    public void testOnboardingWithNoTabs() {
        BaseOnboardingCoordinator coordinator = createCoordinator();
        showOnboardingAndWait(coordinator, mCallback);

        onView(withId(R.id.button_init_not_ok))
                .check(matches(withContentDescription(R.string.cancel)));
        onView(withId(R.id.button_init_ok))
                .check(matches(withContentDescription(R.string.init_ok)));

        onView(withId(R.id.button_init_ok)).perform(click());

        verify(mCallback).onResult(AssistantOnboardingResult.ACCEPTED);
    }

    @Test
    @MediumTest
    public void testTransferControls() throws Exception {
        BaseOnboardingCoordinator coordinator = createCoordinator();

        List<AssistantOverlayCoordinator> capturedOverlays =
                Collections.synchronizedList(new ArrayList<>());
        showOnboardingAndWait(
                coordinator, (result) -> { capturedOverlays.add(coordinator.transferControls()); });

        onView(withId(R.id.button_init_ok)).perform(click());
        assertFalse(TestThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));

        // An overlay was captured, and it is still shown.
        onView(is(mScrimCoordinator.getViewForTesting())).check(matches(isDisplayed()));
        assertEquals(1, capturedOverlays.size());
        AssistantOverlayCoordinator overlay = capturedOverlays.get(0);
        assertNotNull(overlay);
        assertEquals(
                AssistantOverlayState.FULL, overlay.getModel().get(AssistantOverlayModel.STATE));

        // The bottom sheet content is still the assistant one.
        assertThat(mBottomSheetController.getCurrentSheetContent(),
                instanceOf(AssistantBottomSheetContent.class));
    }

    @Test
    @MediumTest
    public void testShownFlag() {
        BaseOnboardingCoordinator coordinator = createCoordinator();
        assertFalse(coordinator.getOnboardingShown());

        showOnboardingAndWait(coordinator, mCallback);
        assertTrue(coordinator.getOnboardingShown());
    }

    @Test
    @MediumTest
    public void testShowDifferentInformationalText() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        HashMap<String, String> parameters = new HashMap();
        parameters.put("INTENT", "RENT_CAR");
        BaseOnboardingCoordinator coordinator = createCoordinator("", parameters);
        showOnboardingAndWait(coordinator, mCallback);

        TextView termsView = mActivity.findViewById(R.id.onboarding_subtitle);
        assertEquals(
                mActivity.getResources().getText(R.string.autofill_assistant_init_message_short),
                termsView.getText());
        TextView titleView = mActivity.findViewById(R.id.onboarding_try_assistant);
        assertEquals(
                mActivity.getResources().getText(R.string.autofill_assistant_init_message_rent_car),
                titleView.getText());
    }

    @Test
    @MediumTest
    public void testShowExperimentalInformationalText() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        HashMap<String, String> parameters = new HashMap();
        parameters.put("INTENT", "BUY_MOVIE_TICKET");
        BaseOnboardingCoordinator coordinator = createCoordinator("4363482", parameters);
        showOnboardingAndWait(coordinator, mCallback);

        TextView termsView = mActivity.findViewById(R.id.onboarding_subtitle);
        assertEquals(
                mActivity.getResources().getText(R.string.autofill_assistant_init_message_short),
                termsView.getText());
        TextView titleView = mActivity.findViewById(R.id.onboarding_try_assistant);
        assertEquals(mActivity.getResources().getText(
                             R.string.autofill_assistant_init_message_buy_movie_tickets),
                titleView.getText());
    }

    @Test
    @MediumTest
    public void testShowStandardInformationalText() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        BaseOnboardingCoordinator coordinator = createCoordinator();
        showOnboardingAndWait(coordinator, mCallback);

        TextView termsView = mActivity.findViewById(R.id.onboarding_subtitle);
        assertEquals(View.VISIBLE, termsView.getVisibility());
        assertEquals(mActivity.getResources().getText(R.string.autofill_assistant_init_message),
                termsView.getText());
        TextView titleView = mActivity.findViewById(R.id.onboarding_try_assistant);
        assertEquals(mActivity.getResources().getText(R.string.autofill_assistant_init_title),
                titleView.getText());
    }

    @Test
    @MediumTest
    public void testUseOfOutsideStrings() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        HashMap<String, String> parameters = new HashMap<>();
        parameters.put("ONBOARDING_FETCH_TIMEOUT_MS", "0");
        BaseOnboardingCoordinator coordinator = createCoordinator("", parameters);

        String expectedTitle = "Title";
        String expectedMessage = "Message";
        String expectedTerms = "Terms <link>Click</link>";
        String expectedTermsUrl = "https://something.google.com/something";

        coordinator.addEntryToStringMap("onboarding_title", expectedTitle);
        coordinator.addEntryToStringMap("onboarding_text", expectedMessage);
        coordinator.addEntryToStringMap("terms_and_conditions", expectedTerms);
        coordinator.addEntryToStringMap("terms_and_conditions_url", expectedTermsUrl);

        showOnboardingAndWait(coordinator, mCallback);

        assertEquals(((TextView) mActivity.findViewById(R.id.onboarding_try_assistant)).getText(),
                expectedTitle);
        assertEquals(((TextView) mActivity.findViewById(R.id.onboarding_subtitle)).getText(),
                expectedMessage);
        TextView termsMessage = mActivity.findViewById(R.id.google_terms_message);
        assertThat(termsMessage.getText().toString(),
                allOf(containsString("Terms"), containsString("Click")));
        Spanned spannedMessage = (Spanned) termsMessage.getText();
        ClickableSpan[] spans =
                spannedMessage.getSpans(0, spannedMessage.length(), ClickableSpan.class);
        assertEquals(spans.length, 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("Click",
                    spannedMessage
                            .subSequence(spannedMessage.getSpanStart(spans[0]),
                                    spannedMessage.getSpanEnd(spans[0]))
                            .toString());
        });
        CustomTabActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                CustomTabActivity.class, Stage.RESUMED, () -> spans[0].onClick(termsMessage));
        CriteriaHelper.pollUiThread(() -> {
            return activity.getActivityTab() != null
                    && activity.getActivityTab().getUrlString().equals(expectedTermsUrl);
        });
        activity.finish();
    }

    @Test
    @MediumTest
    public void testUseOfOutsideStringsRejectsTermsWithoutLink() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        HashMap<String, String> parameters = new HashMap<>();
        parameters.put("ONBOARDING_FETCH_TIMEOUT_MS", "0");
        BaseOnboardingCoordinator coordinator = createCoordinator("", parameters);

        coordinator.addEntryToStringMap("terms_and_conditions", "Bad terms");

        showOnboardingAndWait(coordinator, mCallback);

        TextView termsMessage = mActivity.findViewById(R.id.google_terms_message);
        assertEquals(termsMessage.getText().toString(),
                mActivity.getResources()
                        .getText(R.string.autofill_assistant_google_terms_description)
                        .toString()
                        .replaceAll("<link>", "")
                        .replaceAll("</link>", ""));
    }

    @Test
    @MediumTest
    public void testUseOfOutsideStringsRejectsNonGoogleSudomainLink() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        HashMap<String, String> parameters = new HashMap<>();
        parameters.put("ONBOARDING_FETCH_TIMEOUT_MS", "0");
        BaseOnboardingCoordinator coordinator = createCoordinator("", parameters);

        coordinator.addEntryToStringMap(
                "terms_and_conditions_url", "https://www.domain.com/something");

        showOnboardingAndWait(coordinator, mCallback);

        TextView termsMessage = mActivity.findViewById(R.id.google_terms_message);
        Spanned spannedMessage = (Spanned) termsMessage.getText();
        ClickableSpan[] spans =
                spannedMessage.getSpans(0, spannedMessage.length(), ClickableSpan.class);
        assertEquals(spans.length, 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("Learn more",
                    spannedMessage
                            .subSequence(spannedMessage.getSpanStart(spans[0]),
                                    spannedMessage.getSpanEnd(spans[0]))
                            .toString()
                            .replaceAll("\\s+", " "));
        });
        CustomTabActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                CustomTabActivity.class, Stage.RESUMED, () -> spans[0].onClick(termsMessage));
        String url = mActivity.getResources()
                             .getText(R.string.autofill_assistant_google_terms_url)
                             .toString();
        CriteriaHelper.pollUiThread(() -> {
            return activity.getActivityTab() != null
                    && activity.getActivityTab().getUrlString().equals(url);
        });
        activity.finish();
    }

    /** Trigger onboarding and wait until it is fully displayed. */
    private void showOnboardingAndWait(
            BaseOnboardingCoordinator coordinator, Callback<Integer> callback) {
        TestThreadUtils.runOnUiThreadBlocking(() -> coordinator.show(callback));
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
    }
}
