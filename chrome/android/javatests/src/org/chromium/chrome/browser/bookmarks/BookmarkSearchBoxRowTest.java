// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** Non-render tests for {@link BookmarkSearchBoxRow}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BookmarkSearchBoxRowTest {
    @Rule
    public final DisableAnimationsTestRule mDisableAnimationsRule = new DisableAnimationsTestRule();
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Callback<String> mQueryCallback;
    @Mock
    private Callback<Boolean> mFocusChangeCallback;
    @Mock
    private Callback<Boolean> mToggleCallback;

    private BookmarkSearchBoxRow mBookmarkSearchBoxRow;
    private EditText mEditText;
    private View mShoppingFilterChip;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = mActivityTestRule.getActivity();
            LinearLayout contentView = new LinearLayout(activity);

            // Older Android versions need this otherwise {@link View#clearFocus()} will be ignored.
            // This also mirrors what {@link SelectableListLayout} does.
            contentView.setFocusableInTouchMode(true);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            activity.setContentView(contentView, params);

            LayoutInflater layoutInflater = LayoutInflater.from(activity);
            mBookmarkSearchBoxRow =
                    layoutInflater.inflate(R.layout.bookmark_search_box_row, contentView)
                            .findViewById(R.id.bookmark_toolbar);
            mEditText = mBookmarkSearchBoxRow.findViewById(R.id.search_text);
            mShoppingFilterChip = mBookmarkSearchBoxRow.findViewById(R.id.shopping_filter_chip);

            mPropertyModel =
                    new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                            .with(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, true)
                            .with(BookmarkSearchBoxRowProperties.QUERY_CALLBACK, mQueryCallback)
                            .with(BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK,
                                    mFocusChangeCallback)
                            .with(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK,
                                    mToggleCallback)
                            .build();
            PropertyModelChangeProcessor.create(
                    mPropertyModel, mBookmarkSearchBoxRow, BookmarkSearchBoxRowViewBinder::bind);
        });
    }

    private <T> void setProperty(WritableObjectPropertyKey<T> property, T value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(property, value));
    }

    private void setProperty(WritableBooleanPropertyKey property, boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(property, value));
    }

    @Test
    @MediumTest
    public void testFocusAndEnter() {
        onView(withId(R.id.search_text)).perform(click());
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mEditText.hasFocus(), Matchers.is(true)));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(), mEditText,
                    KeyEvent.KEYCODE_ENTER);
        });
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mEditText.hasFocus(), Matchers.is(false)));
    }

    @Test
    @MediumTest
    public void testQueryCallback() {
        String query = "foo";
        // TODO(https://crbug.com/1467376): Use a model property instead to set the query text.
        TestThreadUtils.runOnUiThreadBlocking(() -> mEditText.setText(query));
        verify(mQueryCallback).onResult(eq(query));
    }

    @Test
    @MediumTest
    public void testFocusChangeCallback() {
        setProperty(BookmarkSearchBoxRowProperties.HAS_FOCUS, true);
        verify(mFocusChangeCallback).onResult(true);

        setProperty(BookmarkSearchBoxRowProperties.HAS_FOCUS, false);
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
    public void testShoppingChipToggle() {
        onView(withId(R.id.shopping_filter_chip)).perform(click());
        verify(mToggleCallback).onResult(true);

        onView(withId(R.id.shopping_filter_chip)).perform(click());
        verify(mToggleCallback).onResult(false);
    }
}
