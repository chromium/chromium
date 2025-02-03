// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ButtonCompat;

/** Unit tests for {@link TabGroupShareNoticeBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupShareNoticeBottomSheetViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Runnable mCompletionHandler;
    private Context mContext;
    private TabGroupShareNoticeBottomSheetView mView;
    private FrameLayout mIllustration;
    private TextView mTitleTextView;
    private TextView mSubtitleTextView;
    private ButtonCompat mConfirmButton;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mView = new TabGroupShareNoticeBottomSheetView(mContext);
        View contentView = mView.getContentView();
        mIllustration =
                contentView.findViewById(R.id.tab_group_share_notice_bottom_sheet_illustration);
        mTitleTextView =
                contentView.findViewById(R.id.tab_group_share_notice_bottom_sheet_title_text);
        mSubtitleTextView =
                contentView.findViewById(R.id.tab_group_share_notice_bottom_sheet_subtitle_text);
        mConfirmButton =
                contentView.findViewById(R.id.tab_group_share_notice_bottom_sheet_confirm_button);
    }

    @Test
    public void testViewsAreInflated() {
        assertNotNull(mIllustration);
        assertNotNull(mTitleTextView);
        assertNotNull(mSubtitleTextView);
        assertNotNull(mConfirmButton);
    }

    @Test
    public void testTextViewsHaveCorrectText() {
        assertEquals(
                mContext.getString(R.string.tab_group_share_notice_bottom_sheet_title),
                mTitleTextView.getText());
        assertEquals(
                mContext.getString(R.string.tab_group_share_notice_bottom_sheet_subtitle),
                mSubtitleTextView.getText());
    }

    @Test
    public void testConfirmButtonHasCorrectText() {
        assertEquals(
                mContext.getString(R.string.tab_group_share_notice_bottom_sheet_button_text),
                mConfirmButton.getText());
    }

    @Test
    public void testCompletionHandlerCalledOnClick() {
        mView.setCompletionHandler(mCompletionHandler);
        mConfirmButton.performClick();
        verify(mCompletionHandler).run();
    }

    @Test
    public void testCompletionHandlerNotCalledWhenNull() {
        mView.setCompletionHandler(null);
        mConfirmButton.performClick();
        verify(mCompletionHandler, never()).run();
    }
}
