// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_STICKY_LAST_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.RootMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ActionBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** View tests for the keyboard accessory component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressWarnings("DoNotMock") // Mocks GURL
@DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN})
public class KeyboardAccessoryViewTest {
    private static final String CUSTOM_ICON_URL = "https://www.example.com/image.png";
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
    private PropertyModel mModel;
    private BlockingQueue<KeyboardAccessoryView> mKeyboardAccessoryView;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock AutofillImageFetcher mMockImageFetcher;
    @Mock Profile mMockProfile;
    private WebPageStation mPage;

    private static class TestTracker implements Tracker {
        private boolean mWasDismissed;
        private @Nullable String mEmittedEvent;

        @Override
        public void notifyEvent(String event) {
            mEmittedEvent = event;
        }

        public @Nullable String getLastEmittedEvent() {
            return mEmittedEvent;
        }

        @Override
        public boolean shouldTriggerHelpUi(String feature) {
            return true;
        }

        @Override
        public TriggerDetails shouldTriggerHelpUiWithSnooze(String feature) {
            return null;
        }

        @Override
        public boolean wouldTriggerHelpUi(String feature) {
            return true;
        }

        @Override
        public boolean hasEverTriggered(String feature, boolean fromWindow) {
            return true;
        }

        @Override
        public int getTriggerState(String feature) {
            return TriggerState.HAS_NOT_BEEN_DISPLAYED;
        }

        @Override
        public void dismissed(String feature) {
            mWasDismissed = true;
        }

        @Override
        public void dismissedWithSnooze(String feature, int snoozeAction) {
            mWasDismissed = true;
        }

        public boolean wasDismissed() {
            return mWasDismissed;
        }

        @Nullable
        @Override
        public DisplayLockHandle acquireDisplayLock() {
            return () -> {};
        }

        @Override
        public void setPriorityNotification(String feature) {}

        @Override
        public @Nullable String getPendingPriorityNotification() {
            return null;
        }

        @Override
        public void registerPriorityNotificationHandler(
                String feature, Runnable priorityNotificationHandler) {}

        @Override
        public void unregisterPriorityNotificationHandler(String feature) {}

        @Override
        public boolean isInitialized() {
            return true;
        }

        @Override
        public void addOnInitializedCallback(Callback<Boolean> callback) {
            throw new AssertionError("Implement addOnInitializedCallback if you need it.");
        }
    }

    @After
    public void tearDown() {
        mActivityTestRule.skipWindowAndTabStateCleanup();
    }

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
        AutofillImageFetcherFactory.setInstanceForTesting(mMockImageFetcher);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel =
                            KeyboardAccessoryProperties.defaultModelBuilder()
                                    .with(
                                            SHEET_OPENER_ITEM,
                                            new SheetOpenerBarItem(
                                                    new KeyboardAccessoryButtonGroupCoordinator
                                                            .SheetOpenerCallbacks() {
                                                        @Override
                                                        public void onViewBound(View buttons) {}

                                                        @Override
                                                        public void onViewUnbound(View buttons) {}
                                                    }))
                                    .with(DISABLE_ANIMATIONS_FOR_TESTING, true)
                                    .with(OBFUSCATED_CHILD_AT_CALLBACK, unused -> {})
                                    .with(SHOW_SWIPING_IPH, false)
                                    .with(HAS_STICKY_LAST_ITEM, true)
                                    .build();
                    AsyncViewStub viewStub =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.keyboard_accessory_stub);

                    mKeyboardAccessoryView = new ArrayBlockingQueue<>(1);
                    ViewProvider<KeyboardAccessoryView> provider =
                            AsyncViewProvider.of(viewStub, R.id.keyboard_accessory);
                    LazyConstructionPropertyMcp.create(
                            mModel, VISIBLE, provider, KeyboardAccessoryViewBinder::bind);
                    provider.whenLoaded(
                            (view) -> {
                                KeyboardAccessoryViewBinder.UiConfiguration uiConfiguration =
                                        KeyboardAccessoryCoordinator.createUiConfiguration(
                                                mActivityTestRule.getActivity(), mMockImageFetcher);
                                view.setBarItemsAdapter(
                                        KeyboardAccessoryCoordinator.createBarItemsAdapter(
                                                mModel.get(BAR_ITEMS), view, uiConfiguration));
                                mKeyboardAccessoryView.add(view);
                            });
                });
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel() throws InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mKeyboardAccessoryView.poll());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();
        assertEquals(View.VISIBLE, view.getVisibility());

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, false);
                });
        assertNotEquals(view.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testAccessoryDimensions() throws InterruptedException {
        assertNull(mKeyboardAccessoryView.poll());
        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();

        LinearLayout barContents = view.findViewById(R.id.accessory_bar_contents);
        assertThat(
                barContents.getMinimumHeight(),
                is(getDimensionPixelSize(R.dimen.keyboard_accessory_height)));
        LinearLayout.LayoutParams params =
                (LinearLayout.LayoutParams) barContents.getLayoutParams();
        assertThat(params.height, is(getDimensionPixelSize(R.dimen.keyboard_accessory_height)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN})
    public void testAccessoryDimensionsWithRedesign() throws InterruptedException {
        assertNull(mKeyboardAccessoryView.poll());
        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();

        LinearLayout barContents = view.findViewById(R.id.accessory_bar_contents);
        assertThat(
                barContents.getMinimumHeight(),
                is(getDimensionPixelSize(R.dimen.keyboard_accessory_height_redesign)));
        LinearLayout.LayoutParams params =
                (LinearLayout.LayoutParams) barContents.getLayoutParams();
        assertThat(
                params.height,
                is(getDimensionPixelSize(R.dimen.keyboard_accessory_height_redesign)));
    }

    @Test
    @MediumTest
    public void testClicksWhileViewObscuredNotAllowed() throws InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mKeyboardAccessoryView.poll());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                });
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(false));
    }

    @Test
    @MediumTest
    public void testAddsClickableAutofillSuggestions() {
        AtomicReference<Boolean> clickRecorded = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    createAutofillChipAndTab(
                                            "Johnathan", result -> clickRecorded.set(true)));
                });

        onViewWaiting(withText("Johnathan")).perform(click());

        assertTrue(clickRecorded.get());
    }

    @Test
    @MediumTest
    public void testGroupedSuggestionsAreClickable() {
        AtomicReference<Boolean> clickRecorded1 = new AtomicReference<>();
        AtomicReference<Boolean> clickRecorded2 = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        createAutofillBarItem(
                                                "Johnathan", result -> clickRecorded1.set(true)),
                                        createAutofillBarItem(
                                                "Mark", result -> clickRecorded2.set(true)),
                                        createSheetOpener()
                                    });
                });

        onViewWaiting(withText("Johnathan")).perform(click());
        assertTrue(clickRecorded1.get());

        onViewWaiting(withText("Mark")).perform(click());
        assertTrue(clickRecorded2.get());
    }

    @Test
    @MediumTest
    public void testAddsLongClickableAutofillSuggestions() {
        AtomicReference<Boolean> clickRecorded = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        new AutofillBarItem(
                                                new AutofillSuggestion.Builder()
                                                        .setLabel("Johnathan")
                                                        .setSubLabel("Smith")
                                                        .setSuggestionType(
                                                                SuggestionType.ADDRESS_ENTRY)
                                                        .setFeatureForIph("")
                                                        .setApplyDeactivatedStyle(false)
                                                        .build(),
                                                new Action(
                                                        AUTOFILL_SUGGESTION,
                                                        result -> {},
                                                        result -> clickRecorded.set(true)),
                                                mMockProfile),
                                        createSheetOpener()
                                    });
                });

        onViewWaiting(withText("Johnathan")).perform(longClick());

        assertTrue(clickRecorded.get());
    }

    @Test
    @MediumTest
    public void testCanAddSingleButtons() {
        BarItem generatePasswordItem =
                new ActionBarItem(
                        BarItem.Type.ACTION_BUTTON,
                        new Action(GENERATE_PASSWORD_AUTOMATIC, unused -> {}),
                        R.string.password_generation_accessory_button);
        BarItem credmanItem =
                new ActionBarItem(
                        BarItem.Type.ACTION_CHIP,
                        new Action(CREDMAN_CONDITIONAL_UI_REENTRY, unused -> {}),
                        R.string.more_passkeys);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        generatePasswordItem, credmanItem, createSheetOpener()
                                    });
                });

        onViewWaiting(withText(R.string.password_generation_accessory_button));
        onViewWaiting(withText(R.string.more_passkeys));
        onView(withText(R.string.password_generation_accessory_button))
                .check(matches(isDisplayed()));
        onView(withText(R.string.more_passkeys)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCanRemoveSingleButtons() {
        BarItem generatePasswordsItem =
                new ActionBarItem(
                        BarItem.Type.ACTION_BUTTON,
                        new Action(GENERATE_PASSWORD_AUTOMATIC, unused -> {}),
                        R.string.password_generation_accessory_button);
        BarItem credmanItem =
                new ActionBarItem(
                        BarItem.Type.ACTION_CHIP,
                        new Action(CREDMAN_CONDITIONAL_UI_REENTRY, unused -> {}),
                        R.string.more_passkeys);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        generatePasswordsItem, credmanItem, createSheetOpener()
                                    });
                });

        onViewWaiting(withText(R.string.password_generation_accessory_button));
        onView(withText(R.string.password_generation_accessory_button))
                .check(matches(isDisplayed()));
        onView(withText(R.string.more_passkeys)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.get(BAR_ITEMS).remove(mModel.get(BAR_ITEMS).get(1)));

        ViewUtils.waitForViewCheckingState(
                withText(R.string.more_passkeys), VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL);
        onView(withText(R.string.password_generation_accessory_button))
                .check(matches(isDisplayed()));
        onView(withText(R.string.more_passkeys)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testUpdatesKeyPaddingAfterRotation() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(createAutofillChipAndTab("John", null));
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();
        CriteriaHelper.pollUiThread(
                () -> view.mBarItemsView.isShown() && view.mBarItemsView.getChildAt(1) != null);
        CriteriaHelper.pollUiThread(viewsAreRightAligned(view, view.mBarItemsView.getChildAt(1)));

        rotateActivityToLandscape();

        CriteriaHelper.pollUiThread(view.mBarItemsView::isShown);
        CriteriaHelper.pollUiThread(viewsAreRightAligned(view, view.mBarItemsView.getChildAt(1)));

        // Reset device orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    public void testDismissesCardInfoRetrievalBubbleOnFilling() throws InterruptedException {
        String descriptionText =
                "You can autofill this card because your PayPay account is linked to Google";
        AutofillBarItem itemWithIph =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Card Info Retrieval")
                                .setSubLabel("")
                                .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                                .setIphDescriptionText(descriptionText)
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);
        itemWithIph.setFeatureForIph(
                FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_CARD_INFO_RETRIEVAL_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIph, createSheetOpener()});
                });

        onViewWaiting(withText("Card Info Retrieval"));
        waitForHelpBubble(withText(descriptionText));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        onView(withChild(withText("Card Info Retrieval"))).check(matches(isSelected()));
        onView(withText("Card Info Retrieval")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(
                tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PAYMENT_CARD_INFO_RETRIEVAL_AUTOFILLED));
        onView(withChild(withText("Card Info Retrieval"))).check(matches(not(isSelected())));
    }

    @Test
    @MediumTest
    public void testDismissesHomeAndWorkdEducationBubbleOnFilling() throws InterruptedException {
        AutofillBarItem itemWithIph =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                                .setFeatureForIph("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);
        itemWithIph.setFeatureForIph(
                FeatureConstants.KEYBOARD_ACCESSORY_HOME_WORK_PROFILE_SUGGESTION_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIph, createSheetOpener()});
                });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_home_work_profile_suggestion));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(
                tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_HOME_AND_WORK_ADDRESS_AUTOFILLED));
    }

    @Test
    @MediumTest
    public void testDismissesPasswordEducationBubbleOnFilling() throws InterruptedException {
        AutofillBarItem itemWithIph =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setSuggestionType(SuggestionType.PASSWORD_ENTRY)
                                .setFeatureForIph("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);
        itemWithIph.setFeatureForIph(FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIph, createSheetOpener()});
                });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_fill_with_chrome));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        onView(withChild(withText("Johnathan"))).check(matches(isSelected()));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(
                tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PASSWORD_AUTOFILLED));
        onView(withChild(withText("Johnathan"))).check(matches(not(isSelected())));
    }

    @Test
    @MediumTest
    public void testDismissesAddressEducationBubbleOnFilling() throws InterruptedException {
        AutofillBarItem itemWithIph =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                                .setFeatureForIph("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);
        itemWithIph.setFeatureForIph(FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIph, createSheetOpener()});
                });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_fill_with_chrome));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(
                tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_ADDRESS_AUTOFILLED));
    }

    @Test
    @MediumTest
    public void testDismissesPaymentEducationBubbleOnFilling() throws InterruptedException {
        AutofillBarItem itemWithIph =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                                .setFeatureForIph("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);
        itemWithIph.setFeatureForIph(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIph, createSheetOpener()});
                });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_fill_with_chrome));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(
                tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PAYMENT_AUTOFILLED));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "crbug.com/40263973")
    public void testDismissesSwipingEducationBubbleOnTap() throws InterruptedException {
        TestTracker tracker =
                new TestTracker() {
                    @Override
                    public int getTriggerState(String feature) {
                        // Pretend that an autofill IPH was shown already.
                        return feature.equals(
                                        FeatureConstants
                                                .KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE)
                                ? TriggerState.HAS_BEEN_DISPLAYED
                                : TriggerState.HAS_NOT_BEEN_DISPLAYED;
                    }
                };
        TrackerFactory.setTrackerForTests(tracker);

        // Render a keyboard accessory bar and wait for completion.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(createAutofillChipAndTab("Johnathan", null));
                });
        onViewWaiting(withText("Johnathan"));

        // Pretend an item is offscreen, so swiping is possible and an IPH could be shown.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(SHOW_SWIPING_IPH, true));

        // Wait until the bubble appears, then dismiss is by tapping it.
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_swipe_for_more));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_swipe_for_more))
                .perform(click());
        assertThat(tracker.wasDismissed(), is(true));
    }

    @Test
    @MediumTest
    public void testDismissesPaymentOfferEducationBubbleOnFilling() throws InterruptedException {
        AutofillBarItem itemWithIph =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setIconId(R.drawable.ic_offer_tag)
                                .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                                .setFeatureForIph("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);
        itemWithIph.setFeatureForIph(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIph, createSheetOpener()});
                });

        onViewWaiting(withText("Johnathan"));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(
                tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PAYMENT_AUTOFILLED));
    }

    @Test
    @MediumTest
    public void testNotifiesAboutPartiallyVisibleSuggestions() throws InterruptedException {
        // Ensure that the callback isn't triggered while all items are visible:
        AtomicInteger obfuscatedChildAt = new AtomicInteger(-1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(OBFUSCATED_CHILD_AT_CALLBACK, obfuscatedChildAt::set);
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(createAutofillChipAndTab("John", null));
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();
        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        assertThat(obfuscatedChildAt.get(), is(-1));

        // As soon as at least one item can't be displayed in full, trigger the swiping callback.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        createAutofillBarItem("JohnathanSmith", null),
                                        createAutofillBarItem("TroyMcSpartanGregor", null),
                                        createAutofillBarItem("SomeOtherRandomLongName", null),
                                        createAutofillBarItem("ToddTester", null),
                                        createAutofillBarItem("MayaPark", null),
                                        createAutofillBarItem("ThisChipIsProbablyHiddenNow", null),
                                        createSheetOpener()
                                    });
                });
        onViewWaiting(withText("JohnathanSmith"));
        CriteriaHelper.pollUiThread(() -> obfuscatedChildAt.get() > -1);
    }

    @Test
    @MediumTest
    public void testCustomIconUrlSet_imageReturnedByImageFetcher_customIconSetOnChipView()
            throws InterruptedException {
        GURL customIconUrl = mock(GURL.class);
        when(customIconUrl.isValid()).thenReturn(true);
        when(customIconUrl.getSpec()).thenReturn(CUSTOM_ICON_URL);
        // Return the cached image when AutofillImageFetcher.getImageIfAvailable is called for the
        // above url.
        when(mMockImageFetcher.getImageIfAvailable(any(), any())).thenReturn(TEST_CARD_ART_IMAGE);
        // Create an autofill suggestion and set the `customIconUrl`.
        AutofillBarItem customIconItem =
                new AutofillBarItem(
                        getDefaultAutofillSuggestionBuilder()
                                .setCustomIconUrl(customIconUrl)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {customIconItem, createSheetOpener()});
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();

        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        CriteriaHelper.pollUiThread(
                () -> {
                    ChipView chipView = (ChipView) view.mBarItemsView.getChildAt(0);
                    ChromeImageView iconImageView = (ChromeImageView) chipView.getChildAt(0);
                    return ((BitmapDrawable) iconImageView.getDrawable())
                            .getBitmap()
                            .equals(TEST_CARD_ART_IMAGE);
                });
    }

    @Test
    @MediumTest
    public void testCustomIconUrlSet_imageNotCachedInImageFetcher_defaultIconSetOnChipView()
            throws InterruptedException {
        GURL customIconUrl = mock(GURL.class);
        when(customIconUrl.isValid()).thenReturn(true);
        when(customIconUrl.getSpec()).thenReturn(CUSTOM_ICON_URL);
        // Return the response of PersonalDataManager.getImageIfAvailable
        // to null to indicate that the image is not present in the cache.
        when(mMockImageFetcher.getImageIfAvailable(any(), any())).thenReturn(null);
        AutofillBarItem customIconItem =
                new AutofillBarItem(
                        getDefaultAutofillSuggestionBuilder()
                                .setCustomIconUrl(customIconUrl)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {customIconItem, createSheetOpener()});
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();

        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        CriteriaHelper.pollUiThread(
                () -> {
                    ChipView chipView = (ChipView) view.mBarItemsView.getChildAt(0);
                    ChromeImageView iconImageView = (ChromeImageView) chipView.getChildAt(0);
                    Drawable expectedIcon =
                            mActivityTestRule.getActivity().getDrawable(R.drawable.visa_card);
                    return getBitmap(expectedIcon).sameAs(getBitmap(iconImageView.getDrawable()));
                });
    }

    @Test
    @MediumTest
    public void testCustomIconUrlNotSet_defaultIconSetOnChipView() throws InterruptedException {
        // Create an autofill suggestion without setting the `customIconUrl`.
        AutofillBarItem itemWithoutCustomIconUrl =
                new AutofillBarItem(
                        getDefaultAutofillSuggestionBuilder().build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}),
                        mMockProfile);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(new BarItem[] {itemWithoutCustomIconUrl, createSheetOpener()});
                });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();

        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        CriteriaHelper.pollUiThread(
                () -> {
                    ChipView chipView = (ChipView) view.mBarItemsView.getChildAt(0);
                    ChromeImageView iconImageView = (ChromeImageView) chipView.getChildAt(0);
                    Drawable expectedIcon =
                            mActivityTestRule.getActivity().getDrawable(R.drawable.visa_card);
                    return getBitmap(expectedIcon).sameAs(getBitmap(iconImageView.getDrawable()));
                });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/385200981")
    public void testClickDisabledForNonAcceptableAutofillSuggestions() throws InterruptedException {
        AtomicReference<Boolean> clickRecorded = new AtomicReference<>(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS)
                            .set(
                                    new BarItem[] {
                                        new AutofillBarItem(
                                                new AutofillSuggestion.Builder()
                                                        .setLabel("Virtual Card")
                                                        .setSubLabel("Disabled")
                                                        .setSuggestionType(
                                                                SuggestionType.CREDIT_CARD_ENTRY)
                                                        .setFeatureForIph("")
                                                        .setApplyDeactivatedStyle(true)
                                                        .build(),
                                                new Action(
                                                        AUTOFILL_SUGGESTION,
                                                        result -> clickRecorded.set(true),
                                                        result -> clickRecorded.set(true)),
                                                mMockProfile),
                                        createSheetOpener()
                                    });
                });

        onView(withText("Virtual Card")).perform(click());
        assertFalse(clickRecorded.get());
        onView(withText("Virtual Card")).check(matches(not(isSelected())));
    }

    @Test
    @MediumTest
    public void testAccessoryButtonsHaveHoverBackground() throws InterruptedException {
        KeyboardAccessoryButtonGroupView buttonGroupView = setupButtonsAndGetGroup();
        ArrayList<ImageButton> buttons = buttonGroupView.getButtons();
        assertEquals("Expected two buttons to be present.", 2, buttons.size());

        // The presence of a background drawable (e.g., a state-list drawable)
        // is used to enable visual feedback on hover and press states.
        assertNotNull(
                "First button should have a background for hover effects.",
                buttons.get(0).getBackground());
        assertNotNull(
                "Second button should have a background for hover effects.",
                buttons.get(1).getBackground());
    }

    @Test
    @MediumTest
    public void testAccessoryButtonsHaveCorrectSpacing() throws InterruptedException {
        KeyboardAccessoryButtonGroupView buttonGroupView = setupButtonsAndGetGroup();
        ArrayList<ImageButton> buttons = buttonGroupView.getButtons();
        assertEquals("Expected two buttons to be present.", 2, buttons.size());

        ImageButton button1 = buttons.get(0);
        ImageButton button2 = buttons.get(1);

        // Check spacing.
        ViewGroup.MarginLayoutParams params1 =
                (ViewGroup.MarginLayoutParams) button1.getLayoutParams();
        ViewGroup.MarginLayoutParams params2 =
                (ViewGroup.MarginLayoutParams) button2.getLayoutParams();

        int expectedMargin =
                buttonGroupView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.keyboard_accessory_tab_icon_spacing);

        assertEquals(
                "First button's left margin is incorrect.", expectedMargin, params1.leftMargin);
        assertEquals(
                "First button's right margin is incorrect.", expectedMargin, params1.rightMargin);
        assertEquals(
                "Second button's left margin is incorrect.", expectedMargin, params2.leftMargin);
        assertEquals(
                "Second button's right margin is incorrect.", expectedMargin, params2.rightMargin);
    }

    /**
     * Sets up the accessory, adds two buttons, and waits for them to be laid out.
     *
     * @return The {@link KeyboardAccessoryButtonGroupView} containing the buttons.
     */
    private KeyboardAccessoryButtonGroupView setupButtonsAndGetGroup() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {createSheetOpener()});
                });

        // Wait for the view and find the KeyboardAccessoryButtonGroupView.
        mKeyboardAccessoryView.take(); // Make sure the view is inflated.
        AtomicReference<KeyboardAccessoryButtonGroupView> buttonGroupViewRef =
                new AtomicReference<>();
        onView(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .check(
                        (v, noViewFoundException) -> {
                            if (noViewFoundException != null) {
                                throw new RuntimeException(noViewFoundException);
                            }
                            buttonGroupViewRef.set((KeyboardAccessoryButtonGroupView) v);
                        });

        KeyboardAccessoryButtonGroupView buttonGroupView = buttonGroupViewRef.get();
        assertNotNull(buttonGroupView);

        // Wait for buttons to be added.
        CriteriaHelper.pollUiThread(() -> buttonGroupView.getButtons().size() == 2);

        return buttonGroupView;
    }

    private static AutofillSuggestion.Builder getDefaultAutofillSuggestionBuilder() {
        return new AutofillSuggestion.Builder()
                .setLabel("Johnathan")
                .setSubLabel("Smith")
                .setIconId(R.drawable.visa_card)
                .setSuggestionType(SuggestionType.ADDRESS_ENTRY);
    }

    // Convert a drawable to a Bitmap for comparison.
    private static Bitmap getBitmap(Drawable drawable) {
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    private int getDimensionPixelSize(@DimenRes int res) {
        return mActivityTestRule.getActivity().getResources().getDimensionPixelSize(res);
    }

    private ViewInteraction waitForHelpBubble(Matcher<View> matcher) {
        View mainDecorView = mActivityTestRule.getActivity().getWindow().getDecorView();
        return onView(isRoot())
                .inRoot(RootMatchers.withDecorView(not(is(mainDecorView))))
                .check(ViewUtils.isEventuallyVisible(matcher));
    }

    private void rotateActivityToLandscape() {
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String result =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        mActivityTestRule.getWebContents(),
                                        "screen.orientation.type.split('-')[0]");
                        Criteria.checkThat(result, is("\"landscape\""));
                    } catch (TimeoutException ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                });
    }

    private Runnable viewsAreRightAligned(View staticView, View changingView) {
        Rect accessoryViewRect = new Rect();
        staticView.getGlobalVisibleRect(accessoryViewRect);
        return () -> {
            Rect keyItemRect = new Rect();
            changingView.getGlobalVisibleRect(keyItemRect);
            Criteria.checkThat(keyItemRect.right, is(accessoryViewRect.right));
        };
    }

    private BarItem[] createAutofillChipAndTab(String label, Callback<Action> chipCallback) {
        return new BarItem[] {createAutofillBarItem(label, chipCallback), createSheetOpener()};
    }

    private AutofillBarItem createAutofillBarItem(String label, Callback<Action> chipCallback) {
        return new AutofillBarItem(
                new AutofillSuggestion.Builder()
                        .setLabel(label)
                        .setSubLabel("Smith")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIph("")
                        .setApplyDeactivatedStyle(false)
                        .build(),
                new Action(AUTOFILL_SUGGESTION, chipCallback),
                mMockProfile);
    }

    private SheetOpenerBarItem createSheetOpener() {
        return new SheetOpenerBarItem(
                new KeyboardAccessoryButtonGroupCoordinator.SheetOpenerCallbacks() {
                    @Override
                    public void onViewBound(View buttons) {
                        KeyboardAccessoryButtonGroupView group =
                                (KeyboardAccessoryButtonGroupView) buttons;
                        if (group.getButtons().size() > 0) {
                            return;
                        }

                        group.addButton(
                                buttons.getContext()
                                        .getDrawable(R.drawable.ic_password_manager_key),
                                "Key Icon");

                        group.addButton(
                                buttons.getContext().getDrawable(R.drawable.ic_credit_card_black),
                                "Card Icon 2");
                    }

                    @Override
                    public void onViewUnbound(View buttons) {}
                });
    }
}
