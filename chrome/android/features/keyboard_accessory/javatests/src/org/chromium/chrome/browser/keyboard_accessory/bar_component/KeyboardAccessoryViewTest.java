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
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.RootMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
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

import java.util.Optional;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** View tests for the keyboard accessory component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressWarnings("DoNotMock") // Mocks GURL
public class KeyboardAccessoryViewTest {
    private static final String CUSTOM_ICON_URL = "https://www.example.com/image.png";
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
    private PropertyModel mModel;
    private BlockingQueue<KeyboardAccessoryView> mKeyboardAccessoryView;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock PersonalDataManager mMockPersonalDataManager;

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
        public boolean shouldTriggerHelpUI(String feature) {
            return true;
        }

        @Override
        public TriggerDetails shouldTriggerHelpUIWithSnooze(String feature) {
            return null;
        }

        @Override
        public boolean wouldTriggerHelpUI(String feature) {
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
            assert false : "Implement addOnInitializedCallback if you need it.";
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
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
                                                mActivityTestRule.getActivity(),
                                                mMockPersonalDataManager);
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
        assertEquals(view.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, false);
                });
        assertNotEquals(view.getVisibility(), View.VISIBLE);
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
                                                        .setItemTag("")
                                                        .setSuggestionType(
                                                                SuggestionType.ADDRESS_ENTRY)
                                                        .setFeatureForIPH("")
                                                        .setApplyDeactivatedStyle(false)
                                                        .build(),
                                                new Action(
                                                        AUTOFILL_SUGGESTION,
                                                        result -> {},
                                                        result -> clickRecorded.set(true))),
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
                new BarItem(
                        BarItem.Type.ACTION_BUTTON,
                        new Action(GENERATE_PASSWORD_AUTOMATIC, unused -> {}),
                        R.string.password_generation_accessory_button);
        BarItem credmanItem =
                new BarItem(
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
                new BarItem(
                        BarItem.Type.ACTION_BUTTON,
                        new Action(GENERATE_PASSWORD_AUTOMATIC, unused -> {}),
                        R.string.password_generation_accessory_button);
        BarItem credmanItem =
                new BarItem(
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
    public void testDismissesPlusAddressEducationBubbleOnFilling() throws InterruptedException {
        AutofillBarItem itemWithIPH =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Create plus address")
                                .setSubLabel("")
                                .setItemTag("")
                                .setSuggestionType(SuggestionType.CREATE_NEW_PLUS_ADDRESS)
                                .setFeatureForIPH("")
                                .setIPHDescriptionText("IPH description")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(
                FeatureConstants.KEYBOARD_ACCESSORY_PLUS_ADDRESS_CREATE_SUGGESTION);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
                });

        onViewWaiting(withText("Create plus address"));
        waitForHelpBubble(withText("IPH description"));
        assertThat(mKeyboardAccessoryView.take().areClicksAllowedWhenObscured(), is(true));
        onView(withChild(withText("Create plus address"))).check(matches(isSelected()));
        onView(withText("Create plus address")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(
                tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PLUS_ADDRESS_CREATE_SUGGESTION));
        onView(withChild(withText("Create plus address"))).check(matches(not(isSelected())));
    }

    @Test
    @MediumTest
    public void testDismissesPasswordEducationBubbleOnFilling() throws InterruptedException {
        AutofillBarItem itemWithIPH =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setItemTag("")
                                .setSuggestionType(SuggestionType.PASSWORD_ENTRY)
                                .setFeatureForIPH("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
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
        AutofillBarItem itemWithIPH =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setItemTag("")
                                .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                                .setFeatureForIPH("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
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
        AutofillBarItem itemWithIPH =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setItemTag("")
                                .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                                .setFeatureForIPH("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
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
        String itemTag = "Cashback linked";
        AutofillBarItem itemWithIPH =
                new AutofillBarItem(
                        new AutofillSuggestion.Builder()
                                .setLabel("Johnathan")
                                .setSubLabel("Smith")
                                .setItemTag(itemTag)
                                .setIconId(R.drawable.ic_offer_tag)
                                .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                                .setFeatureForIPH("")
                                .setApplyDeactivatedStyle(false)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE, true);
                    mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
                });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(itemTag));
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
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)
    public void testCustomIconUrlSet_imageReturnedByPersonalDataManager_customIconSetOnChipView()
            throws InterruptedException {
        GURL customIconUrl = mock(GURL.class);
        when(customIconUrl.isValid()).thenReturn(true);
        when(customIconUrl.getSpec()).thenReturn(CUSTOM_ICON_URL);
        // Return the cached image when
        // PersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable is called for the
        // above url.
        when(mMockPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(any(), any()))
                .thenReturn(Optional.of(TEST_CARD_ART_IMAGE));
        // Create an autofill suggestion and set the `customIconUrl`.
        AutofillBarItem customIconItem =
                new AutofillBarItem(
                        getDefaultAutofillSuggestionBuilder()
                                .setCustomIconUrl(customIconUrl)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));

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
    public void testCustomIconUrlSet_imageNotCachedInPersonalDataManager_defaultIconSetOnChipView()
            throws InterruptedException {
        GURL customIconUrl = mock(GURL.class);
        when(customIconUrl.isValid()).thenReturn(true);
        when(customIconUrl.getSpec()).thenReturn(CUSTOM_ICON_URL);
        // Return the response of PersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable
        // to null to indicate that the image is not present in the cache.
        when(mMockPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(any(), any()))
                .thenReturn(Optional.empty());
        AutofillBarItem customIconItem =
                new AutofillBarItem(
                        getDefaultAutofillSuggestionBuilder()
                                .setCustomIconUrl(customIconUrl)
                                .build(),
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));

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
                        new Action(AUTOFILL_SUGGESTION, unused -> {}));

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
                                                        .setItemTag("")
                                                        .setSuggestionType(
                                                                SuggestionType.CREDIT_CARD_ENTRY)
                                                        .setFeatureForIPH("")
                                                        .setApplyDeactivatedStyle(true)
                                                        .build(),
                                                new Action(
                                                        AUTOFILL_SUGGESTION,
                                                        result -> clickRecorded.set(true),
                                                        result -> clickRecorded.set(true))),
                                        createSheetOpener()
                                    });
                });

        onView(withText("Virtual Card")).perform(click());
        assertFalse(clickRecorded.get());
        onView(withText("Virtual Card")).check(matches(not(isSelected())));
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
                        .setItemTag("")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIPH("")
                        .setApplyDeactivatedStyle(false)
                        .build(),
                new Action(AUTOFILL_SUGGESTION, chipCallback));
    }

    private SheetOpenerBarItem createSheetOpener() {
        return new SheetOpenerBarItem(
                new KeyboardAccessoryButtonGroupCoordinator.SheetOpenerCallbacks() {
                    @Override
                    public void onViewBound(View buttons) {
                        if (((KeyboardAccessoryButtonGroupView) buttons).getButtons().size() > 0) {
                            return;
                        }
                        ((KeyboardAccessoryButtonGroupView) buttons)
                                .addButton(
                                        buttons.getContext()
                                                .getDrawable(R.drawable.ic_password_manager_key),
                                        "Key Icon");
                    }

                    @Override
                    public void onViewUnbound(View buttons) {}
                });
    }
}
