// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_TITLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.url.GURL;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the keyboard accessory component.
 *
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
@SuppressWarnings("DoNotMock") // Mocks GURL
public class KeyboardAccessoryModernViewTest {
    private static final String CUSTOM_ICON_URL = "https://www.example.com/image.png";
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
    private PropertyModel mModel;
    private BlockingQueue<KeyboardAccessoryModernView> mKeyboardAccessoryView;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    PersonalDataManager mMockPersonalDataManager;

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
        @Nullable
        public String getPendingPriorityNotification() {
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
        PersonalDataManager.setInstanceForTesting(mMockPersonalDataManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel = KeyboardAccessoryProperties.defaultModelBuilder()
                             .with(SHEET_OPENER_ITEM,
                                     new SheetOpenerBarItem(
                                             new KeyboardAccessoryTabLayoutCoordinator
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
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_stub);

            mKeyboardAccessoryView = new ArrayBlockingQueue<>(1);
            ViewProvider<KeyboardAccessoryModernView> provider =
                    AsyncViewProvider.of(viewStub, R.id.keyboard_accessory);
            LazyConstructionPropertyMcp.create(
                    mModel, VISIBLE, provider, KeyboardAccessoryModernViewBinder::bind);
            provider.whenLoaded(mKeyboardAccessoryView::add);
        });
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel() throws InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mKeyboardAccessoryView.poll());

        // After setting the visibility to true, the view should exist and be visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, true); });
        KeyboardAccessoryModernView view = mKeyboardAccessoryView.take();
        assertEquals(view.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, false); });
        assertNotEquals(view.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testAddsClickableAutofillSuggestions() {
        AtomicReference<Boolean> clickRecorded = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(
                    createAutofillChipAndTab("Johnathan", result -> clickRecorded.set(true)));
        });

        onViewWaiting(withText("Johnathan")).perform(click());

        assertTrue(clickRecorded.get());
    }

    @Test
    @MediumTest
    public void testAddsLongClickableAutofillSuggestions() {
        AtomicReference<Boolean> clickRecorded = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {
                    new AutofillBarItem(
                            new AutofillSuggestion("Johnathan", "Smith", "", DropdownItem.NO_ICON,
                                    false, 1, false, false, false, /* featureForIPH= */ ""),
                            new Action("Unused", AUTOFILL_SUGGESTION,
                                    result -> {}, result -> clickRecorded.set(true))),
                    createSheetOpener()});
        });

        onViewWaiting(withText("Johnathan")).perform(longClick());

        assertTrue(clickRecorded.get());
    }

    @Test
    @MediumTest
    public void testUpdatesKeyPaddingAfterRotation() throws InterruptedException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.set(KEYBOARD_TOGGLE_VISIBLE, false);
            mModel.set(SHEET_TITLE, "Sheet title");
            mModel.get(BAR_ITEMS).set(createAutofillChipAndTab("John", null));
        });
        KeyboardAccessoryModernView view = mKeyboardAccessoryView.take();
        CriteriaHelper.pollUiThread(view.mBarItemsView::isShown);
        CriteriaHelper.pollUiThread(viewsAreRightAligned(view, view.mBarItemsView.getChildAt(1)));

        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(KEYBOARD_TOGGLE_VISIBLE, true));
        CriteriaHelper.pollUiThread(() -> !view.mBarItemsView.isShown());

        rotateActivityToLandscape();
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(KEYBOARD_TOGGLE_VISIBLE, false));

        CriteriaHelper.pollUiThread(view.mBarItemsView::isShown);
        CriteriaHelper.pollUiThread(viewsAreRightAligned(view, view.mBarItemsView.getChildAt(1)));

        // Reset device orientation.
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    public void testDismissesPasswordEducationBubbleOnFilling() {
        AutofillBarItem itemWithIPH = new AutofillBarItem(
                new AutofillSuggestion("Johnathan", "Smith", /*itemTag=*/"", DropdownItem.NO_ICON,
                        false, -2, false, false, false, /* featureForIPH= */ ""),
                new KeyboardAccessoryData.Action("", AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
        });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_fill_with_chrome));
        onView(withChild(withText("Johnathan"))).check(matches(isSelected()));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PASSWORD_AUTOFILLED));
        onView(withChild(withText("Johnathan"))).check(matches(not(isSelected())));
    }

    @Test
    @MediumTest
    public void testDismissesAddressEducationBubbleOnFilling() {
        AutofillBarItem itemWithIPH = new AutofillBarItem(
                new AutofillSuggestion("Johnathan", "Smith", /*itemTag=*/"", DropdownItem.NO_ICON,
                        false, 1, false, false, false, /* featureForIPH= */ ""),
                new KeyboardAccessoryData.Action("", AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
        });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_fill_with_chrome));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_ADDRESS_AUTOFILLED));
    }

    @Test
    @MediumTest
    public void testDismissesPaymentEducationBubbleOnFilling() {
        AutofillBarItem itemWithIPH = new AutofillBarItem(
                new AutofillSuggestion("Johnathan", "Smith", /*itemTag=*/"", DropdownItem.NO_ICON,
                        false, 70000, false, false, false, /* featureForIPH= */ ""),
                new KeyboardAccessoryData.Action("", AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
        });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_fill_with_chrome));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PAYMENT_AUTOFILLED));
    }

    @Test
    @MediumTest
    public void testDismissesSwipingEducationBubbleOnTap() {
        TestTracker tracker = new TestTracker() {
            @Override
            public int getTriggerState(String feature) {
                // Pretend that an autofill IPH was shown already.
                return feature.equals(FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE)
                        ? TriggerState.HAS_BEEN_DISPLAYED
                        : TriggerState.HAS_NOT_BEEN_DISPLAYED;
            }
        };
        TrackerFactory.setTrackerForTests(tracker);

        // Render a keyboard accessory bar and wait for completion.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(createAutofillChipAndTab("Johnathan", null));
        });
        onViewWaiting(withText("Johnathan"));

        // Pretend an item is offscreen, so swiping is possible and an IPH could be shown.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(SHOW_SWIPING_IPH, true));

        // Wait until the bubble appears, then dismiss is by tapping it.
        waitForHelpBubble(withText(R.string.iph_keyboard_accessory_swipe_for_more))
                .perform(click());
        assertThat(tracker.wasDismissed(), is(true));
    }

    @Test
    @MediumTest
    public void testDismissesPaymentOfferEducationBubbleOnFilling() {
        String itemTag = "Cashback linked";
        AutofillBarItem itemWithIPH = new AutofillBarItem(
                new AutofillSuggestion("Johnathan", "Smith", itemTag, R.drawable.ic_offer_tag,
                        false, 70000, false, false, false, /* featureForIPH= */ ""),
                new KeyboardAccessoryData.Action("", AUTOFILL_SUGGESTION, unused -> {}));
        itemWithIPH.setFeatureForIPH(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE);

        TestTracker tracker = new TestTracker();
        TrackerFactory.setTrackerForTests(tracker);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {itemWithIPH, createSheetOpener()});
        });

        onViewWaiting(withText("Johnathan"));
        waitForHelpBubble(withText(itemTag));
        onView(withText("Johnathan")).perform(click());

        assertThat(tracker.wasDismissed(), is(true));
        assertThat(tracker.getLastEmittedEvent(),
                is(EventConstants.KEYBOARD_ACCESSORY_PAYMENT_AUTOFILLED));
    }

    @Test
    @MediumTest
    public void testNotifiesAboutPartiallyVisibleSuggestions() throws InterruptedException {
        // Ensure that the callback isn't triggered while all items are visible:
        AtomicInteger obfuscatedChildAt = new AtomicInteger(-1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(OBFUSCATED_CHILD_AT_CALLBACK, obfuscatedChildAt::set);
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(createAutofillChipAndTab("John", null));
        });
        KeyboardAccessoryModernView view = mKeyboardAccessoryView.take();
        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        assertThat(obfuscatedChildAt.get(), is(-1));

        // As soon as at least one item can't be displayed in full, trigger the swiping callback.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(BAR_ITEMS).set(new BarItem[] {createAutofillBarItem("JohnathanSmith", null),
                    createAutofillBarItem("TroyMcSpartanGregor", null),
                    createAutofillBarItem("SomeOtherRandomLongName", null),
                    createAutofillBarItem("ToddTester", null),
                    createAutofillBarItem("MayaPark", null),
                    createAutofillBarItem("ThisChipIsProbablyHiddenNow", null),
                    createSheetOpener()});
        });
        onViewWaiting(withText("JohnathanSmith"));
        CriteriaHelper.pollUiThread(() -> obfuscatedChildAt.get() > -1);
    }

    @Test
    @MediumTest
    public void testCustomIconUrlSet_imageReturnedByPersonalDataManager_customIconSetOnChipView()
            throws InterruptedException {
        GURL customIconUrl = mock(GURL.class);
        when(customIconUrl.isValid()).thenReturn(true);
        when(customIconUrl.getSpec()).thenReturn(CUSTOM_ICON_URL);
        // Return the cached image when
        // PersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable is called for the
        // above url.
        when(mMockPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(any()))
                .thenReturn(TEST_CARD_ART_IMAGE);
        // Create an autofill suggestion and set the `customIconUrl`.
        AutofillBarItem customIconItem = new AutofillBarItem(
                getDefaultAutofillSuggestionBuilder().setCustomIconUrl(customIconUrl).build(),
                new KeyboardAccessoryData.Action("", AUTOFILL_SUGGESTION, unused -> {}));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {customIconItem, createSheetOpener()});
        });
        KeyboardAccessoryModernView view = mKeyboardAccessoryView.take();

        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        CriteriaHelper.pollUiThread(() -> {
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
        when(mMockPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(any()))
                .thenReturn(null);
        AutofillBarItem customIconItem = new AutofillBarItem(
                getDefaultAutofillSuggestionBuilder().setCustomIconUrl(customIconUrl).build(),
                new KeyboardAccessoryData.Action("", AUTOFILL_SUGGESTION, unused -> {}));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(new BarItem[] {customIconItem, createSheetOpener()});
        });
        KeyboardAccessoryModernView view = mKeyboardAccessoryView.take();

        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        CriteriaHelper.pollUiThread(() -> {
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
                new AutofillBarItem(getDefaultAutofillSuggestionBuilder().build(),
                        new KeyboardAccessoryData.Action("", AUTOFILL_SUGGESTION, unused -> {}));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(BAR_ITEMS).set(
                    new BarItem[] {itemWithoutCustomIconUrl, createSheetOpener()});
        });
        KeyboardAccessoryModernView view = mKeyboardAccessoryView.take();

        CriteriaHelper.pollUiThread(() -> view.mBarItemsView.getChildCount() > 0);
        CriteriaHelper.pollUiThread(() -> {
            ChipView chipView = (ChipView) view.mBarItemsView.getChildAt(0);
            ChromeImageView iconImageView = (ChromeImageView) chipView.getChildAt(0);
            Drawable expectedIcon =
                    mActivityTestRule.getActivity().getDrawable(R.drawable.visa_card);
            return getBitmap(expectedIcon).sameAs(getBitmap(iconImageView.getDrawable()));
        });
    }

    private static AutofillSuggestion.Builder getDefaultAutofillSuggestionBuilder() {
        return new AutofillSuggestion.Builder()
                .setLabel("Johnathan")
                .setSubLabel("Smith")
                .setIconId(R.drawable.visa_card)
                .setSuggestionId(70000);
    }

    // Convert a drawable to a Bitmap for comparison.
    private static Bitmap getBitmap(Drawable drawable) {
        Bitmap bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    private ViewInteraction waitForHelpBubble(Matcher<View> matcher) {
        View mainDecorView = mActivityTestRule.getActivity().getWindow().getDecorView();
        return onView(isRoot())
                .inRoot(RootMatchers.withDecorView(not(is(mainDecorView))))
                .check(waitForView(matcher));
    }

    private void rotateActivityToLandscape() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                String result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
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
                new AutofillSuggestion(label, "Smith", /*itemTag=*/"", DropdownItem.NO_ICON, false,
                        1, false, false, false, /* featureForIPH= */ ""),
                new KeyboardAccessoryData.Action("Unused", AUTOFILL_SUGGESTION, chipCallback));
    }

    private SheetOpenerBarItem createSheetOpener() {
        return new SheetOpenerBarItem(
                new KeyboardAccessoryTabLayoutCoordinator.SheetOpenerCallbacks() {
                    @Override
                    public void onViewBound(View buttons) {
                        if (((KeyboardAccessoryButtonGroupView) buttons).getButtons().size() > 0) {
                            return;
                        }
                        ((KeyboardAccessoryButtonGroupView) buttons)
                                .addButton(buttons.getContext().getDrawable(
                                                   R.drawable.ic_vpn_key_grey),
                                        "Key Icon");
                    }

                    @Override
                    public void onViewUnbound(View buttons) {}
                });
    }
}
