// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link NtpThemeListThemeCollectionItemIconView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeListThemeCollectionItemIconViewUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private NtpThemeListThemeCollectionItemIconView mView;

    private View mNoImagePlaceholder;
    private ImageView mPrimaryImage;
    private View mSecondaryImageContainer;
    private ImageView mSecondaryImage;
    private View mBottomRightContainer;
    private View mBottomRightBackground;
    private ImageView mBottomRightIcon;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                (NtpThemeListThemeCollectionItemIconView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout
                                                .ntp_customization_theme_collections_list_item_icon_layout,
                                        null,
                                        false);

        mNoImagePlaceholder = mView.findViewById(R.id.no_image_placeholder_background);
        mPrimaryImage = mView.findViewById(R.id.primary_image);
        mSecondaryImageContainer = mView.findViewById(R.id.secondary_image_container);
        mSecondaryImage = mView.findViewById(R.id.secondary_image);
        mBottomRightContainer = mView.findViewById(R.id.bottom_right_container);
        mBottomRightBackground = mView.findViewById(R.id.bottom_right_background);
        mBottomRightIcon = mView.findViewById(R.id.bottom_right_icon);
    }

    @Test
    public void testSetImageDrawables() {
        Drawable primaryDrawable = new ColorDrawable();
        Drawable secondaryDrawable = new ColorDrawable();

        mView.setImageDrawables(primaryDrawable, secondaryDrawable);

        assertEquals(View.GONE, mNoImagePlaceholder.getVisibility());
        assertEquals(View.VISIBLE, mPrimaryImage.getVisibility());
        assertEquals(View.VISIBLE, mSecondaryImageContainer.getVisibility());
        assertEquals(View.VISIBLE, mBottomRightContainer.getVisibility());
        assertEquals(View.VISIBLE, mBottomRightBackground.getVisibility());
        assertEquals(View.VISIBLE, mBottomRightIcon.getVisibility());

        assertEquals(primaryDrawable, mPrimaryImage.getDrawable());
        assertEquals(secondaryDrawable, mSecondaryImage.getDrawable());
    }

    @Test
    public void testSetImageDrawablePair() {
        NtpThemeListThemeCollectionItemIconView spiedView = spy(mView);
        Drawable primaryDrawable = new ColorDrawable();
        Drawable secondaryDrawable = new ColorDrawable();
        Pair<Drawable, Drawable> drawablePair = new Pair<>(primaryDrawable, secondaryDrawable);

        spiedView.setImageDrawablePair(drawablePair);

        verify(spiedView).setImageDrawables(primaryDrawable, secondaryDrawable);
    }
}
