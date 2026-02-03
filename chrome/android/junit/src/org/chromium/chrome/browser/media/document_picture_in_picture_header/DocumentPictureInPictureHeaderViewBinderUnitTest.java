// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.core.graphics.Insets;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link DocumentPictureInPictureHeaderViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureHeaderViewBinderUnitTest {
    private Context mContext;
    private ViewGroup mHeaderView;
    private ImageView mBackToTabButton;
    private ImageView mSecurityIcon;
    private UrlBar mUrlBar;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mHeaderView = spy(new FrameLayout(mContext));
        mHeaderView.setLayoutParams(
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0));

        mSecurityIcon = mock(ImageView.class);
        mBackToTabButton = mock(ImageView.class);
        mUrlBar = mock(UrlBar.class);
        doReturn(mSecurityIcon)
                .when(mHeaderView)
                .findViewById(R.id.document_picture_in_picture_header_security_icon);
        doReturn(mBackToTabButton)
                .when(mHeaderView)
                .findViewById(R.id.document_picture_in_picture_header_back_to_tab);
        doReturn(mUrlBar)
                .when(mHeaderView)
                .findViewById(R.id.document_picture_in_picture_header_url_bar);

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
    public void testTintColorList() {
        ColorStateList tint = ColorStateList.valueOf(Color.RED);
        mModel.set(DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST, tint);

        verify(mBackToTabButton).setImageTintList(tint);
        verify(mSecurityIcon).setImageTintList(tint);
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

    @Test
    @SmallTest
    public void testNonDraggableAreas() {
        List<Rect> rects = new ArrayList<>();
        rects.add(new Rect(0, 0, 10, 10));
        mModel.set(DocumentPictureInPictureHeaderProperties.NON_DRAGGABLE_AREAS, rects);

        verify(mHeaderView).setSystemGestureExclusionRects(rects);
    }

    @Test
    @SmallTest
    public void testBackToTabClickListener() {
        View.OnClickListener listener = v -> {};
        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER, listener);
        verify(mBackToTabButton).setOnClickListener(listener);
    }

    @Test
    @SmallTest
    public void testIsBackToTabShown() {
        mModel.set(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN, true);
        verify(mBackToTabButton).setVisibility(View.VISIBLE);

        mModel.set(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN, false);
        verify(mBackToTabButton).setVisibility(View.GONE);
    }

    @Test
    @SmallTest
    public void testSecurityIcon() {
        int iconRes = 123;
        mModel.set(DocumentPictureInPictureHeaderProperties.SECURITY_ICON, iconRes);
        verify(mSecurityIcon).setImageResource(iconRes);
    }

    @Test
    @SmallTest
    public void testSecurityIconClickListener() {
        View.OnClickListener listener = v -> {};
        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_SECURITY_ICON_CLICK_LISTENER, listener);
        verify(mSecurityIcon).setOnClickListener(listener);
    }

    @Test
    @SmallTest
    public void testUrlHost() {
        String host = JUnitTestGURLs.EXAMPLE_URL.getHost();
        mModel.set(DocumentPictureInPictureHeaderProperties.URL_STRING, host);
        verify(mUrlBar).setTextWithTruncation(host, ScrollType.NO_SCROLL, -1);
        verify(mUrlBar).setTooltipText(host);
    }

    @Test
    @SmallTest
    public void testBrandedColorScheme() {
        mModel.set(
                DocumentPictureInPictureHeaderProperties.BRANDED_COLOR_SCHEME,
                BrandedColorScheme.APP_DEFAULT);
        verify(mUrlBar).setTextColor(anyInt());
    }
}
