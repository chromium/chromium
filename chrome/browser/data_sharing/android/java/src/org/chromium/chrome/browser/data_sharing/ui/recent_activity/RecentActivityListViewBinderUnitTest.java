// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private OnClickListener mOnClickListener;
    @Mock private Drawable mDrawable;
    private Context mContext;
    private ViewGroup mListRowView;
    private TextView mTitleView;
    private TextView mDescriptionView;
    private ImageView mFaviconView;
    private ImageView mAvatarView;
    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mContext = spy(new ContextWrapper(activity));
        mListRowView = new FrameLayout(mContext);
        mTitleView = new TextView(mContext);
        mTitleView.setId(R.id.title);
        mDescriptionView = new TextView(mContext);
        mDescriptionView.setId(R.id.description);
        mFaviconView = new ImageView(mContext);
        mFaviconView.setId(R.id.favicon);
        mAvatarView = new ImageView(mContext);
        mAvatarView.setId(R.id.avatar);
        mListRowView.addView(mTitleView);
        mListRowView.addView(mDescriptionView);
        mListRowView.addView(mFaviconView);
        mListRowView.addView(mAvatarView);
        activity.setContentView(mListRowView);

        mPropertyModel = new PropertyModel.Builder(RecentActivityListProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mListRowView, RecentActivityListViewBinder::bind);
    }

    @Test
    public void testTitle() {
        final String title = "Title 1";
        mPropertyModel.set(RecentActivityListProperties.TITLE_TEXT, title);
        assertEquals(title, mTitleView.getText().toString());
    }

    @Test
    public void testDescriptionWithTimestamp() {
        String combinedString = "sample full description";
        doReturn(combinedString).when(mContext).getString(anyInt(), any(), any(), any());

        DescriptionAndTimestamp descriptionAndTimestamp =
                new DescriptionAndTimestamp(
                        /* description= */ "description 1",
                        /* separator= */ ".",
                        /* timestamp= */ "8h ago",
                        /* descriptionFullTextResId= */ 5);
        mPropertyModel.set(
                RecentActivityListProperties.DESCRIPTION_AND_TIMESTAMP_TEXT,
                descriptionAndTimestamp);
        ShadowLooper.idleMainLooper();
        assertEquals(combinedString, mDescriptionView.getText().toString());
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
        ShadowLooper.idleMainLooper();
        verify(mContext, never()).getString(anyInt(), any(), any(), any());
        assertEquals(descriptionAndTimestamp.timestamp, mDescriptionView.getText().toString());
    }

    @Test
    public void testFavicon() {
        mFaviconView.setImageDrawable(mock(Drawable.class));
        mPropertyModel.set(
                RecentActivityListProperties.FAVICON_PROVIDER,
                imageView -> {
                    assertNull(imageView.getDrawable());
                    imageView.setImageDrawable(mDrawable);
                });
        assertEquals(mDrawable, mFaviconView.getDrawable());
    }

    @Test
    public void testFavicon_nullProvider() {
        mPropertyModel.set(RecentActivityListProperties.FAVICON_PROVIDER, null);
        assertEquals(View.GONE, mFaviconView.getVisibility());
    }

    @Test
    public void testAvatar() {
        mAvatarView.setImageDrawable(mock(Drawable.class));
        mPropertyModel.set(
                RecentActivityListProperties.AVATAR_PROVIDER,
                imageView -> {
                    assertNull(imageView.getDrawable());
                    imageView.setImageDrawable(mDrawable);
                });
        assertEquals(mDrawable, mAvatarView.getDrawable());
    }

    @Test
    public void testOnClickListener() {
        mPropertyModel.set(RecentActivityListProperties.ON_CLICK_LISTENER, mOnClickListener);
        mListRowView.performClick();
        verify(mOnClickListener, times(1)).onClick(eq(mListRowView));
    }
}
