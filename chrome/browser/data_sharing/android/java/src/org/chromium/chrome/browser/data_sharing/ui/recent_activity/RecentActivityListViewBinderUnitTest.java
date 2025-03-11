// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextPaint;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ViewGroup mListRowView;
    @Mock private TextView mTitleView;
    @Mock private TextView mDescriptionView;
    @Mock private ImageView mFaviconView;
    @Mock private ImageView mAvatarView;
    private PropertyModel mPropertyModel;
    @Mock private OnClickListener mOnClickListener;
    @Mock private Drawable mDrawable;
    @Mock private Context mContext;
    @Mock private TextPaint mTextPaint;
    @Captor private ArgumentCaptor<Runnable> mPostedTask;

    @Before
    public void setup() {
        when(mListRowView.findViewById(R.id.title)).thenReturn(mTitleView);
        when(mListRowView.findViewById(R.id.description)).thenReturn(mDescriptionView);
        when(mListRowView.findViewById(R.id.favicon)).thenReturn(mFaviconView);
        when(mListRowView.findViewById(R.id.avatar)).thenReturn(mAvatarView);
        when(mDescriptionView.getContext()).thenReturn(mContext);
        when(mDescriptionView.getPaint()).thenReturn(mTextPaint);

        mPropertyModel = new PropertyModel.Builder(RecentActivityListProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mListRowView, RecentActivityListViewBinder::bind);
    }

    @Test
    public void testTitle() {
        final String title = "Title 1";
        mPropertyModel.set(RecentActivityListProperties.TITLE_TEXT, title);
        verify(mTitleView).setText(eq(title));
    }

    @Test
    public void testDescriptionWithTimestamp() {
        DescriptionAndTimestamp descriptionAndTimestamp =
                new DescriptionAndTimestamp(
                        /* description= */ "description 1",
                        /* separator= */ ".",
                        /* timestamp= */ "8h ago",
                        /* descriptionFullTextResId= */ 5);
        mPropertyModel.set(
                RecentActivityListProperties.DESCRIPTION_AND_TIMESTAMP_TEXT,
                descriptionAndTimestamp);
        verify(mDescriptionView).post(mPostedTask.capture());

        String combinedString = "sample full description";
        when(mContext.getString(anyInt(), any(), any(), any())).thenReturn(combinedString);

        mPostedTask.getValue().run();
        verify(mDescriptionView, times(1)).setText(eq(combinedString));
    }

    @Test
    public void testTimestampWithEmptyDescription() {
        DescriptionAndTimestamp descriptionAndTimestamp =
                new DescriptionAndTimestamp(
                        /* description= */ "",
                        /* separator= */ ".",
                        /* timestamp= */ "8h ago",
                        /* descriptionFullTextResId= */ 5);
        mPropertyModel.set(
                RecentActivityListProperties.DESCRIPTION_AND_TIMESTAMP_TEXT,
                descriptionAndTimestamp);
        verify(mDescriptionView).post(mPostedTask.capture());

        mPostedTask.getValue().run();
        verify(mContext, never()).getString(anyInt(), any(), any(), any());
        verify(mDescriptionView, times(1)).setText(eq(descriptionAndTimestamp.timestamp));
    }

    @Test
    public void testFavicon() {
        mPropertyModel.set(
                RecentActivityListProperties.FAVICON_PROVIDER,
                imageView -> imageView.setImageDrawable(mDrawable));
        InOrder inOrder = Mockito.inOrder(mFaviconView);
        inOrder.verify(mFaviconView).setImageDrawable(eq(null));
        inOrder.verify(mFaviconView).setImageDrawable(eq(mDrawable));
    }

    @Test
    public void testFavicon_nullProvider() {
        mPropertyModel.set(RecentActivityListProperties.FAVICON_PROVIDER, null);
        InOrder inOrder = Mockito.inOrder(mFaviconView);
        inOrder.verify(mFaviconView).setVisibility(eq(View.GONE));
    }

    @Test
    public void testAvatar() {
        mPropertyModel.set(
                RecentActivityListProperties.AVATAR_PROVIDER,
                imageView -> imageView.setImageDrawable(mDrawable));
        InOrder inOrder = Mockito.inOrder(mAvatarView);
        inOrder.verify(mAvatarView).setImageDrawable(eq(null));
        inOrder.verify(mAvatarView).setImageDrawable(eq(mDrawable));
    }

    @Test
    public void testOnClickListener() {
        mPropertyModel.set(RecentActivityListProperties.ON_CLICK_LISTENER, mOnClickListener);
        verify(mListRowView, times(1)).setOnClickListener(eq(mOnClickListener));
    }
}
