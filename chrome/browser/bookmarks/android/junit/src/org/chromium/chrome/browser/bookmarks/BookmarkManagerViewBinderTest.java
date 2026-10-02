// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BookmarkManagerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkManagerViewBinderTest {
    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private ViewGroup mView;
    private TextView mTextView;
    private PropertyModel mModel;

    @Before
    public void before() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mView = new FrameLayout(mActivity);
        mTextView = new TextView(mActivity);
        mTextView.setId(R.id.title);
        mView.addView(mTextView);
        mModel = new PropertyModel(BookmarkManagerProperties.ALL_KEYS);
    }

    @Test
    public void testConstructor() {
        new BookmarkManagerViewBinder();
    }

    @Test
    public void testBindLegacyPromoView() {
        mModel.set(BookmarkManagerProperties.BOOKMARK_ID, null);
        PropertyModelChangeProcessor.create(
                mModel, mView, BookmarkManagerViewBinder::bindLegacyPromoView);
    }

    @Test
    public void testBindSectionHeaderView() {
        BookmarkListEntry bookmarkListEntry =
                BookmarkListEntry.createSectionHeader(
                        R.string.reading_list_read,
                        R.dimen.bookmark_reading_list_section_header_padding_top);
        mModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry);

        PropertyModelChangeProcessor.create(
                mModel, mView, BookmarkManagerViewBinder::bindSectionHeaderView);

        assertEquals(
                mActivity.getResources().getString(R.string.reading_list_read),
                mTextView.getText());
        int expectedTopPadding =
                (int)
                        mActivity
                                .getResources()
                                .getDimension(
                                        R.dimen.bookmark_reading_list_section_header_padding_top);
        assertEquals(expectedTopPadding, mTextView.getPaddingTop());
    }

    @Test
    public void testBindDividerView() {
        mModel.set(BookmarkManagerProperties.BOOKMARK_ID, null);
        PropertyModelChangeProcessor.create(
                mModel, mView, BookmarkManagerViewBinder::bindDividerView);
    }
}
