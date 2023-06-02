// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
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
}
