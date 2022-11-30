// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AssistantBottomSheetContent;
import org.chromium.components.autofill_assistant.AssistantStaticDependencies;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.components.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.components.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
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
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class BottomSheetOnboardingCoordinatorTest {
    private static final int SPLIT_ONBOARDING_EXPERIMENT_VARIANT_A = 4702489;
    private static final int SPLIT_ONBOARDING_EXPERIMENT_VARIANT_B = 4702490;

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
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
        mActivity = mCustomTabActivityTestRule.getActivity();
        mBottomSheetController = TestThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantUiTestUtil.getBottomSheetController(mActivity));
        mScrimCoordinator = mCustomTabActivityTestRule.getActivity()
                                    .getRootUiCoordinatorForTesting()
                                    .getScrimCoordinator();

        AssistantStaticDependencies staticDependencies = new AssistantStaticDependenciesChrome();
        BrowserContextHandle browserContext =
                TestThreadUtils.runOnUiThreadBlocking(() -> staticDependencies.getBrowserContext());
        mOnboardingCoordinatorFactory = new OnboardingCoordinatorFactory(mActivity,
                mBottomSheetController, browserContext,
                ()
                        -> new AssistantBrowserControlsChrome(
                                mActivity.getBrowserControlsManager()),
                mActivity.getCompositorViewHolderForTesting(),
                staticDependencies.getAccessibilityUtil(), staticDependencies.createInfoPageUtil());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_CONSENT);
            prefService.clearPref(Pref.AUTOFILL_ASSISTANT_ENABLED);
        });
    }

    /** Sets the value of @param preference to @param value. */
    private void setBooleanPref(String preference, boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.setBoolean(preference, value);
        });
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
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT,
                expectedResult != AssistantOnboardingResult.ACCEPTED);

        BaseOnboardingCoordinator coordinator = createCoordinator();
        showOnboardingAndWait(coordinator, mCallback);

        assertTrue(TestThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));
        onView(is(mScrimCoordinator.getViewForTesting())).check(matches(isDisplayed()));
        onView(withId(buttonToClick)).perform(scrollTo(), click());

        verify(mCallback).onResult(expectedResult);
        assertFalse(TestThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            assertEquals(expectedResult == AssistantOnboardingResult.ACCEPTED,
                    prefService.getBoolean(Pref.AUTOFILL_ASSISTANT_CONSENT));
        });
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
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

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
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

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
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

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
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

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
                    && activity.getActivityTab().getUrl().getSpec().equals(expectedTermsUrl);
        });
        activity.finish();
    }

    @Test
    @MediumTest
    public void testUseOfOutsideStringsRejectsTermsWithoutLink() {
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

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
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

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
                    && activity.getActivityTab().getUrl().getSpec().equals(url);
        });
        activity.finish();
    }

    @Test
    @MediumTest
    public void testIgnoreShowingUiAfterCancellation() {
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

        BaseOnboardingCoordinator coordinator = createCoordinator();
        showOnboardingAndWait(coordinator, mCallback);

        // Cancel startup.
        TestThreadUtils.runOnUiThreadBlocking(coordinator::hide);
        waitUntilViewAssertionTrue(withId(R.id.autofill_assistant), doesNotExist(), 3000L);

        TestThreadUtils.runOnUiThreadBlocking(coordinator::updateAndShowView);
        // Does not crash.
    }

    /** Trigger onboarding and wait until it is fully displayed. */
    private void showOnboardingAndWait(
            BaseOnboardingCoordinator coordinator, Callback<Integer> callback) {
        TestThreadUtils.runOnUiThreadBlocking(() -> coordinator.show(callback));
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void testSplitBottomSheetOnboardingVariantA() {
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

        HashMap<String, String> parameters = new HashMap<>();

        BaseOnboardingCoordinator coordinator = createCoordinator(
                String.valueOf(SPLIT_ONBOARDING_EXPERIMENT_VARIANT_A), parameters);

        String expectedTitle = "Title";
        String expectedMessage = "Message";
        String expectedTerms = "Terms <link>Click</link>";
        String expectedTermsUrl = "https://something.google.com/something";
        String expectedCloseBottomsheetText = "Not now";
        String expectedOpenDialogText = "Open dialog";
        String expectedTermsTitle = "TermsTitle";
        String expectedConfirmDialogText = "Accept";
        String expectedCloseDialogText = "Reject";

        // If both are provided, split_onboarding_* should take precedence
        coordinator.addEntryToStringMap("onboarding_title", "Should be ignored");
        coordinator.addEntryToStringMap("onboarding_text", "Should be ignored");
        coordinator.addEntryToStringMap("split_onboarding_title", expectedTitle);
        coordinator.addEntryToStringMap("split_onboarding_text", expectedMessage);

        coordinator.addEntryToStringMap("terms_and_conditions", expectedTerms);
        coordinator.addEntryToStringMap("terms_and_conditions_url", expectedTermsUrl);
        coordinator.addEntryToStringMap(
                "split_onboarding_close_bottomsheet", expectedCloseBottomsheetText);
        coordinator.addEntryToStringMap("split_onboarding_show_dialog", expectedOpenDialogText);
        coordinator.addEntryToStringMap("split_onboarding_terms_title", expectedTermsTitle);
        coordinator.addEntryToStringMap("split_onboarding_accept", expectedConfirmDialogText);
        coordinator.addEntryToStringMap("split_onboarding_decline", expectedCloseDialogText);

        showOnboardingAndWait(coordinator, mCallback);

        onView(withId(R.id.onboarding_try_assistant)).check(matches(withText(expectedTitle)));
        onView(withId(R.id.onboarding_subtitle)).check(matches(withText(expectedMessage)));
        onView(withId(R.id.google_terms_message)).check(matches(not(isDisplayed())));
        onView(withId(R.id.onboarding_separator)).check(matches(not(isDisplayed())));
        onView(withId(R.id.button_init_ok)).check(matches(withText(expectedOpenDialogText)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(withText(expectedCloseBottomsheetText)));
        onView(withId(R.id.button_init_ok))
                .check(matches(withContentDescription(expectedOpenDialogText)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(withContentDescription(expectedCloseBottomsheetText)));

        onView(withText(expectedOpenDialogText)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.google_terms_message), isDisplayed());

        onView(withId(R.id.onboarding_try_assistant))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(allOf(isDisplayed(), withText(expectedTermsTitle))));
        onView(withId(R.id.onboarding_subtitle))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(not(isDisplayed())));
        onView(withText(expectedConfirmDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        onView(withText(expectedCloseDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        onView(withId(R.id.google_terms_message))
                .check(matches(allOf(
                        withText(containsString("Terms")), withText(containsString("Click")))));

        // Dismissing the dialog should not dismiss the bottom sheet, nor should the callback be
        // called.
        onView(withText(expectedCloseDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .perform(scrollTo(), click());
        waitUntilViewAssertionTrue(
                withText(expectedTermsTitle), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        onView(withText(expectedOpenDialogText)).check(matches(isDisplayed()));

        onView(withText(expectedOpenDialogText)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.google_terms_message), isDisplayed());

        onView(withText(expectedConfirmDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .perform(scrollTo(), click());
        verify(mCallback).onResult(AssistantOnboardingResult.ACCEPTED);
    }

    @Test
    @MediumTest
    public void testSplitBottomSheetOnboardingVariantAFallbackStrings() {
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

        HashMap<String, String> parameters = new HashMap<>();
        BaseOnboardingCoordinator coordinator = createCoordinator(
                String.valueOf(SPLIT_ONBOARDING_EXPERIMENT_VARIANT_A), parameters);
        showOnboardingAndWait(coordinator, mCallback);

        onView(withId(R.id.onboarding_try_assistant))
                .check(matches(withText(R.string.autofill_assistant_split_onboarding_title)));
        onView(withId(R.id.onboarding_subtitle))
                .check(matches(withText(R.string.autofill_assistant_split_onboarding_subtitle)));
        onView(withId(R.id.google_terms_message)).check(matches(not(isDisplayed())));
        onView(withId(R.id.onboarding_separator)).check(matches(not(isDisplayed())));
        onView(withId(R.id.button_init_ok))
                .check(matches(withText(R.string.autofill_assistant_split_onboarding_show_dialog)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(
                        withText(R.string.autofill_assistant_split_onboarding_close_bottomsheet)));
        onView(withId(R.id.button_init_ok))
                .check(matches(withContentDescription(
                        R.string.autofill_assistant_split_onboarding_show_dialog)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(withContentDescription(
                        R.string.autofill_assistant_split_onboarding_close_bottomsheet)));

        onView(withText(R.string.autofill_assistant_split_onboarding_show_dialog)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.google_terms_message), isDisplayed());

        onView(withId(R.id.onboarding_try_assistant))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(allOf(isDisplayed(),
                        withText(R.string.autofill_assistant_split_onboarding_terms_title))));
        onView(withId(R.id.onboarding_subtitle))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(not(isDisplayed())));
        onView(allOf(withText(R.string.init_ok), isDisplayed()))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        onView(allOf(withText(R.string.cancel), isDisplayed()))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSplitBottomSheetOnboardingVariantB() {
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

        HashMap<String, String> parameters = new HashMap<>();
        BaseOnboardingCoordinator coordinator = createCoordinator(
                String.valueOf(SPLIT_ONBOARDING_EXPERIMENT_VARIANT_B), parameters);

        String expectedTitle = "Title";
        String expectedMessage = "Message";
        String expectedTerms = "Terms <link>Click</link>";
        String expectedTermsUrl = "https://something.google.com/something";
        String expectedCloseBottomsheetText = "Not now";
        String expectedOpenDialogText = "Open dialog";
        String expectedTermsTitle = "TermsTitle";
        String expectedConfirmDialogText = "Accept";
        String expectedCloseDialogText = "Reject";

        // If both are provided, split_onboarding_* should take precedence
        coordinator.addEntryToStringMap("onboarding_title", "Should be ignored");
        coordinator.addEntryToStringMap("onboarding_text", "Should be ignored");
        coordinator.addEntryToStringMap("split_onboarding_title", expectedTitle);
        coordinator.addEntryToStringMap("split_onboarding_text", expectedMessage);

        coordinator.addEntryToStringMap("terms_and_conditions", expectedTerms);
        coordinator.addEntryToStringMap("terms_and_conditions_url", expectedTermsUrl);
        coordinator.addEntryToStringMap(
                "split_onboarding_close_bottomsheet", expectedCloseBottomsheetText);
        coordinator.addEntryToStringMap("split_onboarding_show_dialog", expectedOpenDialogText);
        coordinator.addEntryToStringMap("split_onboarding_terms_title", expectedTermsTitle);
        coordinator.addEntryToStringMap("split_onboarding_accept", expectedConfirmDialogText);
        coordinator.addEntryToStringMap("split_onboarding_decline", expectedCloseDialogText);

        showOnboardingAndWait(coordinator, mCallback);

        // Subtitle is shown in the text bubble, not in the bottom sheet.
        onView(withId(R.id.onboarding_subtitle)).check(matches(not(isDisplayed())));
        onView(withText(expectedMessage))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));

        onView(withId(R.id.onboarding_try_assistant)).check(matches(withText(expectedTitle)));
        onView(withId(R.id.google_terms_message)).check(matches(not(isDisplayed())));
        onView(withId(R.id.onboarding_separator)).check(matches(not(isDisplayed())));
        onView(withId(R.id.button_init_ok)).check(matches(withText(expectedOpenDialogText)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(withText(expectedCloseBottomsheetText)));
        onView(withId(R.id.button_init_ok))
                .check(matches(withContentDescription(expectedOpenDialogText)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(withContentDescription(expectedCloseBottomsheetText)));

        onView(withText(expectedOpenDialogText)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.google_terms_message), isDisplayed());

        onView(withId(R.id.onboarding_try_assistant))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(allOf(isDisplayed(), withText(expectedTermsTitle))));
        onView(withId(R.id.onboarding_subtitle))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(not(isDisplayed())));
        onView(withText(expectedConfirmDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        onView(withText(expectedCloseDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        onView(withId(R.id.google_terms_message))
                .check(matches(allOf(
                        withText(containsString("Terms")), withText(containsString("Click")))));

        // Dismissing the dialog will not bring the text bubble back.
        onView(withText(expectedCloseDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .perform(scrollTo(), click());
        waitUntilViewAssertionTrue(
                withText(expectedTermsTitle), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        onView(withText(expectedOpenDialogText)).check(matches(isDisplayed()));
        onView(withText(expectedMessage)).check(matches(not(isDisplayed())));

        onView(withText(expectedOpenDialogText)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.google_terms_message), isDisplayed());

        onView(withText(expectedConfirmDialogText))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .perform(scrollTo(), click());
        verify(mCallback).onResult(AssistantOnboardingResult.ACCEPTED);
    }

    @Test
    @MediumTest
    public void testSplitBottomSheetOnboardingVariantBFallbackStrings() {
        setBooleanPref(Pref.AUTOFILL_ASSISTANT_CONSENT, true);

        HashMap<String, String> parameters = new HashMap<>();
        BaseOnboardingCoordinator coordinator = createCoordinator(
                String.valueOf(SPLIT_ONBOARDING_EXPERIMENT_VARIANT_B), parameters);
        showOnboardingAndWait(coordinator, mCallback);

        // Subtitle is shown in the text bubble, not in the bottom sheet.
        onView(withId(R.id.onboarding_subtitle)).check(matches(not(isDisplayed())));
        onView(withText(R.string.autofill_assistant_split_onboarding_subtitle))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));

        onView(withId(R.id.onboarding_try_assistant))
                .check(matches(withText(R.string.autofill_assistant_split_onboarding_title)));
        onView(withId(R.id.google_terms_message)).check(matches(not(isDisplayed())));
        onView(withId(R.id.onboarding_separator)).check(matches(not(isDisplayed())));
        onView(withId(R.id.button_init_ok))
                .check(matches(withText(R.string.autofill_assistant_split_onboarding_show_dialog)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(
                        withText(R.string.autofill_assistant_split_onboarding_close_bottomsheet)));
        onView(withId(R.id.button_init_ok))
                .check(matches(withContentDescription(
                        R.string.autofill_assistant_split_onboarding_show_dialog)));
        onView(withId(R.id.button_init_not_ok))
                .check(matches(withContentDescription(
                        R.string.autofill_assistant_split_onboarding_close_bottomsheet)));

        onView(withText(R.string.autofill_assistant_split_onboarding_show_dialog)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.google_terms_message), isDisplayed());

        onView(withId(R.id.onboarding_try_assistant))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(allOf(isDisplayed(),
                        withText(R.string.autofill_assistant_split_onboarding_terms_title))));
        onView(withId(R.id.onboarding_subtitle))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(not(isDisplayed())));
        onView(allOf(withText(R.string.init_ok), isDisplayed()))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        onView(allOf(withText(R.string.cancel), isDisplayed()))
                .inRoot(withDecorView(not(mActivity.getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }
}
