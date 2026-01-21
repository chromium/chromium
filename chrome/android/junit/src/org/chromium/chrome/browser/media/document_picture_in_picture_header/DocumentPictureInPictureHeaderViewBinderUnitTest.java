// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.graphics.Insets;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link DocumentPictureInPictureHeaderViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureHeaderViewBinderUnitTest {
    private Context mContext;
    private View mHeaderView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mHeaderView = new View(mContext);
        mHeaderView.setLayoutParams(
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0));

        mModel =
                new PropertyModel.Builder(DocumentPictureInPictureHeaderProperties.ALL_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mHeaderView, DocumentPictureInPictureHeaderViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testIsShown() {
        mModel.set(DocumentPictureInPictureHeaderProperties.IS_SHOWN, true);
        assertEquals(View.VISIBLE, mHeaderView.getVisibility());

        mModel.set(DocumentPictureInPictureHeaderProperties.IS_SHOWN, false);
        assertEquals(View.GONE, mHeaderView.getVisibility());
    }

    @Test
    @SmallTest
    public void testBackgroundColor() {
        int color = Color.RED;
        mModel.set(DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR, color);

        ColorDrawable background = (ColorDrawable) mHeaderView.getBackground();
        assertNotNull(background);
        assertEquals(color, background.getColor());
    }

    @Test
    @SmallTest
    public void testHeaderHeight() {
        int height = 123;
        mModel.set(DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT, height);

        assertEquals(height, mHeaderView.getLayoutParams().height);
    }

    @Test
    @SmallTest
    public void testHeaderSpacing() {
        int left = 10;
        int top = 20;
        int right = 30;
        int bottom = 40;
        mModel.set(
                DocumentPictureInPictureHeaderProperties.HEADER_SPACING,
                Insets.of(left, top, right, bottom));
        assertEquals(left, mHeaderView.getPaddingLeft());
        assertEquals(top, mHeaderView.getPaddingTop());
        assertEquals(right, mHeaderView.getPaddingRight());
        assertEquals(bottom, mHeaderView.getPaddingBottom());
    }
}
