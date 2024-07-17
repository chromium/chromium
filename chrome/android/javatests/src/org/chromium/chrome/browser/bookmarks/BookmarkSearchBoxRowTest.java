// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.base.test.util.Criteria.checkThat;

import android.app.Activity;
import android.text.TextUtils;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

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
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<String> mSearchTextChangeCallback;
    @Mock private Runnable mClearSearchTextRunnable;
    @Mock private Callback<Boolean> mFocusChangeCallback;
    @Mock private Callback<Boolean> mToggleCallback;

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
                    activity.setContentView(contentView, params);

                    LayoutInflater layoutInflater = LayoutInflater.from(activity);
                    mBookmarkSearchBoxRow =
                            layoutInflater
                                    .inflate(R.layout.bookmark_search_box_row, contentView)
                                    .findViewById(R.id.bookmark_toolbar);
                    mEditText = mBookmarkSearchBoxRow.findViewById(R.id.row_search_text);
                    mShoppingFilterChip =
                            mBookmarkSearchBoxRow.findViewById(R.id.shopping_filter_chip);

                    mPropertyModel =
                            new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                                    .with(
                                            BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY,
                                            true)
                                    .with(
                                            BookmarkSearchBoxRowProperties
                                                    .SEARCH_TEXT_CHANGE_CALLBACK,
                                            mSearchTextChangeCallback)
                                    .with(
                                            BookmarkSearchBoxRowProperties
                                                    .CLEAR_SEARCH_TEXT_RUNNABLE,
                                            mClearSearchTextRunnable)
                                    .with(
                                            BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK,
                                            mFocusChangeCallback)
                                    .with(
                                            BookmarkSearchBoxRowProperties
                                                    .SHOPPING_CHIP_TOGGLE_CALLBACK,
                                            mToggleCallback)
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
    public void testFocusAndEnter() {
        onView(withId(R.id.row_search_text)).perform(click());
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(true)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    KeyUtils.singleKeyEventView(
                            InstrumentationRegistry.getInstrumentation(),
                            mEditText,
                            KeyEvent.KEYCODE_ENTER);
                });
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(false)));
    }

    @Test
    @MediumTest
    public void testSearchTextAndChangeCallback() {
        String barText = "bar";
        setProperty(BookmarkSearchBoxRowProperties.SEARCH_TEXT, barText);
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.getText(), withText(barText)));
        verifyNoInteractions(mSearchTextChangeCallback);

        String fooText = "foo";
        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.setText(fooText));
        verify(mSearchTextChangeCallback).onResult(eq(fooText));
    }

    @Test
    @MediumTest
    public void testFocusChangeCallback() {
        setProperty(BookmarkSearchBoxRowProperties.HAS_FOCUS, true);
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(true)));
        verifyNoInteractions(mFocusChangeCallback);

        setProperty(BookmarkSearchBoxRowProperties.HAS_FOCUS, false);
        CriteriaHelper.pollUiThread(() -> checkThat(mEditText.hasFocus(), is(false)));
        verifyNoInteractions(mFocusChangeCallback);

        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.requestFocus());
        verify(mFocusChangeCallback).onResult(true);

        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.clearFocus());
        verify(mFocusChangeCallback).onResult(false);
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
    public void testShoppingChipToggleCallback() {
        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED, false);
        onView(withId(R.id.shopping_filter_chip)).perform(click());
        verify(mToggleCallback).onResult(true);

        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED, true);
        onView(withId(R.id.shopping_filter_chip)).perform(click());
        verify(mToggleCallback).onResult(false);
    }

    @Test
    @MediumTest
    public void testTapSearchRowLayoutClearsSearchFocus() {
        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.requestFocus());
        verify(mFocusChangeCallback).onResult(true);

        onView(withId(R.id.bookmark_toolbar)).perform(click());
        verify(mFocusChangeCallback).onResult(false);
    }

    @Test
    @MediumTest
    public void testTogglingChipDoesNotClearSearchFocus() {
        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.requestFocus());
        verify(mFocusChangeCallback).onResult(true);

        onView(withId(R.id.shopping_filter_chip)).perform(click());
        verify(mFocusChangeCallback, never()).onResult(false);

        onView(withId(R.id.shopping_filter_chip)).perform(click());
        verify(mFocusChangeCallback, never()).onResult(false);
    }

    @Test
    @MediumTest
    public void testTapFilterLayoutClearsSearchFocus() {
        ThreadUtils.runOnUiThreadBlocking(() -> mEditText.requestFocus());
        verify(mFocusChangeCallback).onResult(true);

        onView(withChild(withId(R.id.shopping_filter_chip))).perform(click());
        verify(mFocusChangeCallback).onResult(false);
    }

    @Test
    @MediumTest
    public void testClearSearchTextButtonAndRunnable() {
        onView(withId(R.id.clear_text_button)).check(matches(not(isDisplayed())));

        setProperty(BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY, true);
        onView(withId(R.id.clear_text_button)).check(matches(isDisplayed()));

        onView(withId(R.id.clear_text_button)).perform(click());
        verify(mClearSearchTextRunnable).run();
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
        setProperty(BookmarkSearchBoxRowProperties.SEARCH_TEXT, searchText);
        verify(mSearchTextChangeCallback, times(1)).onResult(eq(searchText));
    }
}
