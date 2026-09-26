// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.search.SearchBoxProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Non-render tests for {@link BookmarkSearchBoxRow}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT)
public class BookmarkSearchBoxRowTest {

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
    public void setUp() {
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        LinearLayout contentView = new LinearLayout(activity);

        // Older Android versions need this otherwise {@link View#clearFocus()} will be
        // ignored.
        // This also mirrors what {@link SelectableListLayout} does.
        contentView.setFocusableInTouchMode(true);

        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER;
        activity.setContentView(contentView, params);

        LayoutInflater layoutInflater = LayoutInflater.from(activity);
        mBookmarkSearchBoxRow =
                layoutInflater
                        .inflate(R.layout.bookmark_search_box_row, contentView)
                        .findViewById(R.id.bookmark_toolbar);
        mEditText = mBookmarkSearchBoxRow.findViewById(R.id.search_text);
        mShoppingFilterChip = mBookmarkSearchBoxRow.findViewById(R.id.shopping_filter_chip);

        mPropertyModel =
                new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                        .with(
                                BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES,
                                R.string.price_tracking_bookmarks_filter_title)
                        .with(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, true)
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
                                BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK,
                                mToggleCallbackHelper::notifyCalled)
                        .build();
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                mBookmarkSearchBoxRow,
                BookmarkSearchBoxRowViewBinder.createViewBinder());
    }

    private <T> void setProperty(WritableObjectPropertyKey<T> property, T value) {
        mPropertyModel.set(property, value);
    }

    private void setProperty(WritableBooleanPropertyKey property, boolean value) {
        mPropertyModel.set(property, value);
    }

    @Test
    public void testFocusAndEnter() {
        mEditText.requestFocus();
        assertTrue(mEditText.hasFocus());

        mEditText.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        mEditText.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        assertFalse(mEditText.hasFocus());
    }

    @Test
    public void testSearchTextAndChangeCallback() {
        String barText = "bar";
        setProperty(SearchBoxProperties.SEARCH_TEXT, barText);
        assertEquals(barText, mEditText.getText().toString());
        assertEquals(0, mSearchTextChangeCallbackHelper.getCallCount());

        String fooText = "foo";
        mEditText.setText(fooText);
        assertEquals(fooText, mSearchTextChangeCallbackHelper.getOnlyPayloadBlocking());
    }

    @Test
    public void testFocusChangeCallback() {
        setProperty(SearchBoxProperties.HAS_FOCUS, true);
        assertTrue(mEditText.hasFocus());
        assertEquals(0, mFocusChangeCallbackHelper.getCallCount());

        setProperty(SearchBoxProperties.HAS_FOCUS, false);
        assertFalse(mEditText.hasFocus());
        assertEquals(0, mFocusChangeCallbackHelper.getCallCount());

        mEditText.requestFocus();
        assertTrue(mFocusChangeCallbackHelper.getPayloadByIndexBlocking(0));
        assertEquals(1, mFocusChangeCallbackHelper.getCallCount());

        mEditText.clearFocus();
        assertFalse(mFocusChangeCallbackHelper.getPayloadByIndexBlocking(1));
        assertEquals(2, mFocusChangeCallbackHelper.getCallCount());
    }

    @Test
    public void testShoppingChipVisibility() {
        assertTrue(mShoppingFilterChip.isShown());

        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, false);
        assertFalse(mShoppingFilterChip.isShown());
    }

    @Test
    public void testShoppingChipToggleCallback() {
        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED, false);
        mShoppingFilterChip.performClick();
        assertTrue(mToggleCallbackHelper.getPayloadByIndexBlocking(0));
        assertEquals(1, mToggleCallbackHelper.getCallCount());

        setProperty(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED, true);
        mShoppingFilterChip.performClick();
        assertFalse(mToggleCallbackHelper.getPayloadByIndexBlocking(1));
        assertEquals(2, mToggleCallbackHelper.getCallCount());
    }

    @Test
    public void testTogglingChipDoesNotClearSearchFocus() {
        mEditText.requestFocus();
        assertTrue(mFocusChangeCallbackHelper.getOnlyPayloadBlocking());

        mShoppingFilterChip.performClick();
        assertEquals(1, mFocusChangeCallbackHelper.getCallCount());

        mShoppingFilterChip.performClick();
        assertEquals(1, mFocusChangeCallbackHelper.getCallCount());
    }

    @Test
    public void testClearSearchTextButtonAndRunnable() {
        View clearTextButton = mBookmarkSearchBoxRow.findViewById(R.id.clear_text_button);
        assertFalse(clearTextButton.isShown());

        setProperty(SearchBoxProperties.CLEAR_BUTTON_VISIBILITY, true);
        assertTrue(clearTextButton.isShown());

        clearTextButton.performClick();
        assertEquals(1, mClearSearchTextCallbackHelper.getCallCount());
    }

    @Test
    public void testRebindSingleSearchTextChangeCallback() {
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                mBookmarkSearchBoxRow,
                BookmarkSearchBoxRowViewBinder.createViewBinder());
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                mBookmarkSearchBoxRow,
                BookmarkSearchBoxRowViewBinder.createViewBinder());

        String searchText = "foo";
        mEditText.setText(searchText);
        assertEquals(searchText, mSearchTextChangeCallbackHelper.getOnlyPayloadBlocking());
    }
}
