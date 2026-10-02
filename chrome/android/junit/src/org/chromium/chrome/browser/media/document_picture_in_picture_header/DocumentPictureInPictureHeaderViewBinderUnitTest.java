// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.graphics.Insets;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
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
    private TextView mUrlBar;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mHeaderView = new FrameLayout(mContext);
        mHeaderView.setLayoutParams(
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0));

        mSecurityIcon = new ImageView(mContext);
        mSecurityIcon.setId(R.id.document_picture_in_picture_header_security_icon);
        mHeaderView.addView(mSecurityIcon, new ViewGroup.LayoutParams(0, 0));

        mBackToTabButton = new ImageView(mContext);
        mBackToTabButton.setId(R.id.document_picture_in_picture_header_back_to_tab);
        mHeaderView.addView(mBackToTabButton, new ViewGroup.LayoutParams(0, 0));

        mUrlBar = new TextView(mContext);
        mUrlBar.setId(R.id.document_picture_in_picture_header_url_bar);
        mHeaderView.addView(mUrlBar, new ViewGroup.LayoutParams(0, 0));

        mModel =
                new PropertyModel.Builder(DocumentPictureInPictureHeaderProperties.ALL_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mHeaderView, DocumentPictureInPictureHeaderViewBinder::bind);
    }

    @Test
    public void testIsShown() {
        mModel.set(DocumentPictureInPictureHeaderProperties.IS_SHOWN, true);
        assertEquals(View.VISIBLE, mHeaderView.getVisibility());

        mModel.set(DocumentPictureInPictureHeaderProperties.IS_SHOWN, false);
        assertEquals(View.GONE, mHeaderView.getVisibility());
    }

    @Test
    public void testBackgroundColor() {
        int color = Color.RED;
        mModel.set(DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR, color);

        ColorDrawable background = (ColorDrawable) mHeaderView.getBackground();
        assertNotNull(background);
        assertEquals(color, background.getColor());
    }

    @Test
    public void testTintColorList() {
        ColorStateList tint = ColorStateList.valueOf(Color.RED);
        mModel.set(DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST, tint);

        assertEquals(tint, mBackToTabButton.getImageTintList());
        assertEquals(tint, mSecurityIcon.getImageTintList());
    }

    @Test
    public void testHeaderHeight() {
        int height = 123;
        mModel.set(DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT, height);

        assertEquals(height, mHeaderView.getLayoutParams().height);
    }

    @Test
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
    public void testNonDraggableAreas() {
        List<Rect> rects = new ArrayList<>();
        rects.add(new Rect(0, 0, 10, 10));
        mModel.set(DocumentPictureInPictureHeaderProperties.NON_DRAGGABLE_AREAS, rects);

        assertEquals(rects, mHeaderView.getSystemGestureExclusionRects());
    }

    @Test
    public void testBackToTabClickListener() {
        View.OnClickListener listener = mock(View.OnClickListener.class);
        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER, listener);
        mBackToTabButton.performClick();
        verify(listener).onClick(mBackToTabButton);
    }

    @Test
    public void testIsBackToTabShown() {
        mBackToTabButton.setVisibility(View.GONE);
        mModel.set(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN, true);
        assertEquals(View.VISIBLE, mBackToTabButton.getVisibility());

        mModel.set(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN, false);
        assertEquals(View.GONE, mBackToTabButton.getVisibility());
    }

    @Test
    public void testSecurityIcon() {
        int iconRes = R.drawable.omnibox_info;
        mModel.set(DocumentPictureInPictureHeaderProperties.SECURITY_ICON, iconRes);
        assertEquals(iconRes, shadowOf(mSecurityIcon.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testSecurityIconClickListener() {
        View.OnClickListener listener = mock(View.OnClickListener.class);
        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_SECURITY_ICON_CLICK_LISTENER, listener);
        mSecurityIcon.performClick();
        verify(listener).onClick(mSecurityIcon);
    }

    @Test
    public void testUrlHost() {
        String host = JUnitTestGURLs.EXAMPLE_URL.getHost();
        mModel.set(DocumentPictureInPictureHeaderProperties.URL_STRING, host);
        assertEquals(host, mUrlBar.getText().toString());
        assertEquals(host, mUrlBar.getTooltipText().toString());
    }

    @Test
    public void testBrandedColorScheme() {
        mUrlBar.setTextColor(Color.RED);
        mModel.set(
                DocumentPictureInPictureHeaderProperties.BRANDED_COLOR_SCHEME,
                BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mContext, BrandedColorScheme.APP_DEFAULT),
                mUrlBar.getCurrentTextColor());
    }

    @Test
    public void testUrlEllipsizeBehavior() {
        mModel.set(
                DocumentPictureInPictureHeaderProperties.URL_ELLIPSIZE_BEHAVIOR,
                TextUtils.TruncateAt.START);
        assertEquals(TextUtils.TruncateAt.START, mUrlBar.getEllipsize());

        mModel.set(
                DocumentPictureInPictureHeaderProperties.URL_ELLIPSIZE_BEHAVIOR,
                TextUtils.TruncateAt.END);
        assertEquals(TextUtils.TruncateAt.END, mUrlBar.getEllipsize());
    }

    @Test
    public void testComponentSize() {
        int size = 42;
        mModel.set(DocumentPictureInPictureHeaderProperties.COMPONENT_SIZE, size);

        assertEquals(size, mBackToTabButton.getLayoutParams().width);
        assertEquals(size, mBackToTabButton.getLayoutParams().height);
        assertEquals(size, mSecurityIcon.getLayoutParams().width);
        assertEquals(size, mSecurityIcon.getLayoutParams().height);
        assertEquals(size, mUrlBar.getLayoutParams().height);
    }
}
