// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

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

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
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
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modelutil.PropertyModel;
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
    private Callback<Boolean> mToggleCallback;

    private BookmarkSearchBoxRow mBookmarkSearchBoxRow;

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
        });
    }

    @Test
    @MediumTest
    public void testFocusAndEnter() {
        EditText editText = mBookmarkSearchBoxRow.findViewById(R.id.search_text);

        TestThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(editText));
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(editText.hasFocus(), Matchers.is(true)));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(),
                                editText, KeyEvent.KEYCODE_ENTER));
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(editText.hasFocus(), Matchers.is(false)));
    }

    @Test
    @MediumTest
    public void testQueryCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                        .with(BookmarkSearchBoxRowProperties.QUERY_CALLBACK, mQueryCallback)
                        .build();
        PropertyModelChangeProcessor.create(
                propertyModel, mBookmarkSearchBoxRow, BookmarkSearchBoxRowViewBinder::bind);
        EditText editText = mBookmarkSearchBoxRow.findViewById(R.id.search_text);
        String query = "foo";

        TestThreadUtils.runOnUiThreadBlocking(() -> editText.setText(query));

        Mockito.verify(mQueryCallback).onResult(Mockito.eq(query));
    }

    @Test
    @MediumTest
    public void testShoppingChipVisibility() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                        .with(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, false)
                        .build();
        PropertyModelChangeProcessor.create(
                propertyModel, mBookmarkSearchBoxRow, BookmarkSearchBoxRowViewBinder::bind);

        View shoppingFilterChip = mBookmarkSearchBoxRow.findViewById(R.id.shopping_filter_chip);
        Assert.assertFalse(shoppingFilterChip.isShown());

        propertyModel.set(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, true);
        Assert.assertTrue(shoppingFilterChip.isShown());
    }

    @Test
    @MediumTest
    public void testShoppingChipToggle() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                        .with(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK,
                                mToggleCallback)
                        .build();
        PropertyModelChangeProcessor.create(
                propertyModel, mBookmarkSearchBoxRow, BookmarkSearchBoxRowViewBinder::bind);
        View shoppingFilterChip = mBookmarkSearchBoxRow.findViewById(R.id.shopping_filter_chip);

        TestThreadUtils.runOnUiThreadBlockingNoException(shoppingFilterChip::performClick);
        Mockito.verify(mToggleCallback).onResult(true);

        TestThreadUtils.runOnUiThreadBlockingNoException(shoppingFilterChip::performClick);
        Mockito.verify(mToggleCallback).onResult(false);
    }
}
