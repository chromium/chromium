// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Criteria.checkThat;

import android.app.Activity;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.KeyUtils;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.search.SearchBoxProperties;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/** Non-render tests for {@link BookmarkSearchBoxRow}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BookmarkSearchBoxRowTest {
    /** Needed because CoreMatchers.equalTo does not correctly handle CharSequences. */
    private static Matcher<CharSequence> withText(CharSequence text) {
        return new BaseMatcher<>() {
            @Override
            public boolean matches(Object o) {
                if (!(o instanceof CharSequence)) return false;
                return TextUtils.equals((CharSequence) o, text);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Expected: " + text);
            }
        };
    }

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private final PayloadCallbackHelper<String> mSearchTextChangeCallbackHelper =
            new PayloadCallbackHelper<>();
    private final CallbackHelper mClearSearchTextCallbackHelper = new CallbackHelper();
    private final PayloadCallbackHelper<Boolean> mFocusChangeCallbackHelper =
            new PayloadCallbackHelper<>();
    private final PayloadCallbackHelper<Boolean> mToggleCallbackHelper =
            new PayloadCallbackHelper<>();

    private BookmarkSearchBoxRow mBookmarkSearchBoxRow;
    private EditText mEditText;
    private View mShoppingFilterChip;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    LinearLayout contentView = new LinearLayout(activity);

                    // Older Android versions need this otherwise {@link View#clearFocus()} will be
                    // ignored.
                    // This also mirrors what {@link SelectableListLayout} does.
                    contentView.setFocusableInTouchMode(true);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    params.gravity = Gravity.CENTER;
                    activity.setContentView(contentView, params);

                    LayoutInflater layoutInflater = LayoutInflater.from(activity);
                    mBookmarkSearchBoxRow =
                            layoutInflater
                                    .inflate(R.layout.bookmark_search_box_row, contentView)
                                    .findViewById(R.id.bookmark_toolbar);
                    mEditText = mBookmarkSearchBoxRow.findViewById(R.id.search_text);
                    mShoppingFilterChip =
                            mBookmarkSearchBoxRow.findViewById(R.id.shopping_filter_chip);

                    mPropertyModel =
                            new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                                    .with(
                                            BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES,
                                            R.string.price_tracking_bookmarks_filter_title)
                                    .with(
                                            BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY,
                                            true)
                                    .with(
                                            SearchBoxProperties.TEXT_CHANGED_CALLBACK,
                                            mSearchTextChangeCallbackHelper::notifyCalled)
                                    .with(
                                            SearchBoxProperties.CLEAR_SEARCH_TEXT_RUNNABLE,
                                            mClearSearchTextCallbackHelper::notifyCalled)
                                    .with(
                                            SearchBoxProperties.FOCUS_CHANGED_CALLBACK,
                                            mFocusChangeCallbackHelper::notifyCalled)
                                    .with(
                                            BookmarkSearchBoxRowProperties
                                                    .SHOPPING_CHIP_TOGGLE_CALLBACK,
                                            mToggleCallbackHelper::notifyCalled)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            mPropertyModel,
                            mBookmarkSearchBoxRow,
                            BookmarkSearchBoxRowViewBinder.createViewBinder());
                });
    }

    private <T> void setProperty(WritableObjectPropertyKey<T> property, T value) {
        ThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(property, value));
    }

    private void setProperty(WritableBooleanPropertyKey property, boolean value) {
        ThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(property, value));
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/562625886
    public void testFocusAndEnter() {
        onView(withId(R.id.search_text)).perform(click());
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(true)));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        KeyUtils.singleKeyEventView(
                                InstrumentationRegistry.getInstrumentation(),
                                mEditText,
                                KeyEvent.KEYCODE_ENTER));
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(false)));
    }

    @Test
    @MediumTest
    public void testSearchTextAndChangeCallback() {
        String barText = "bar";
        setProperty(SearchBoxProperties.SEARCH_TEXT, barText);
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.getText(), withText(barText)));
        assertEquals(0, mSearchTextChangeCallbackHelper.getCallCount());

        String fooText = "foo";
        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.setText(fooText));
        assertEquals(fooText, mSearchTextChangeCallbackHelper.getOnlyPayloadBlocking());
    }

    @Test
    @MediumTest
    public void testFocusChangeCallback() {
        setProperty(SearchBoxProperties.HAS_FOCUS, true);
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(true)));
        assertEquals(0, mFocusChangeCallbackHelper.getCallCount());

        setProperty(SearchBoxProperties.HAS_FOCUS, false);
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(false)));
        assertEquals(0, mFocusChangeCallbackHelper.getCallCount());

        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.performClick());
        assertTrue(mFocusChangeCallbackHelper.getPayloadByIndexBlocking(0));
        assertEquals(1, mFocusChangeCallbackHelper.getCallCount());

        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.clearFocus());
        assertFalse(mFocusChangeCallbackHelper.getPayloadByIndexBlocking(1));
        assertEquals(2, mFocusChangeCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testShoppingChipVisibility() {
        assertTrue(mShoppingFilterChip.isShown());

        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, false);
        assertFalse(mShoppingFilterChip.isShown());
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/562625886
    public void testShoppingChipToggleCallback() {
        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED, false);
        onView(withId(R.id.shopping_filter_chip)).perform(click());
        assertTrue(mToggleCallbackHelper.getPayloadByIndexBlocking(0));
        assertEquals(1, mToggleCallbackHelper.getCallCount());

        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED, true);
        onView(withId(R.id.shopping_filter_chip)).perform(click());
        assertFalse(mToggleCallbackHelper.getPayloadByIndexBlocking(1));
        assertEquals(2, mToggleCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testTogglingChipDoesNotClearSearchFocus() {
        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.performClick());
        assertTrue(mFocusChangeCallbackHelper.getOnlyPayloadBlocking());

        onView(withId(R.id.shopping_filter_chip)).perform(click());
        assertEquals(1, mFocusChangeCallbackHelper.getCallCount());

        onView(withId(R.id.shopping_filter_chip)).perform(click());
        assertEquals(1, mFocusChangeCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testClearSearchTextButtonAndRunnable() throws TimeoutException {
        onView(withId(R.id.clear_text_button)).check(matches(not(isDisplayed())));

        setProperty(SearchBoxProperties.CLEAR_BUTTON_VISIBILITY, true);
        onView(withId(R.id.clear_text_button)).check(matches(isDisplayed()));

        onView(withId(R.id.clear_text_button)).perform(click());
        mClearSearchTextCallbackHelper.waitForOnly();
    }

    @Test
    @MediumTest
    public void testRebindSingleSearchTextChangeCallback() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModelChangeProcessor.create(
                            mPropertyModel,
                            mBookmarkSearchBoxRow,
                            BookmarkSearchBoxRowViewBinder.createViewBinder());
                    PropertyModelChangeProcessor.create(
                            mPropertyModel,
                            mBookmarkSearchBoxRow,
                            BookmarkSearchBoxRowViewBinder.createViewBinder());
                });

        String searchText = "foo";
        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.setText(searchText));
        assertEquals(searchText, mSearchTextChangeCallbackHelper.getOnlyPayloadBlocking());
    }
}
