// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.widget.TextViewWithLeading;

/** Unit tests for {@link BottomSheetListItemView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetListItemViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private BottomSheetListItemView mListView;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mListView =
                (BottomSheetListItemView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.bottom_sheet_list_item_view, null, false);
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        String text = "New tab page cards";
        mListView.setTitle(text);
        TextViewWithLeading textView = mListView.findViewById(R.id.title);
        assertEquals(text, textView.getText());
    }

    @Test
    @SmallTest
    public void testSetSubtitle() {
        // Verifies when the subtitle is null, the visibility of the subtitle view is set to GONE.
        TextViewWithLeading subtitleView = mListView.findViewById(R.id.subtitle);
        mListView.setSubtitle(null);
        assertEquals(View.GONE, subtitleView.getVisibility());

        // Verifies when the subtitle is not null, the text is properly set in the subtitleView.
        String text = "Off";
        mListView.setSubtitle(text);
        assertEquals(text, subtitleView.getText());
    }

    @Test
    @SmallTest
    public void testSetNullTrailingIcon() {
        // Verifies when the resId given is null, the visibility of the icon is set to View.GONE.
        ImageView iconView = mListView.findViewById(R.id.trailing_icon);
        mListView.setTrailingIcon(null);
        assertEquals(View.GONE, iconView.getVisibility());
    }

    @Test
    @SmallTest
    public void testSetNonNullTrailingIcon() {
        // Verifies when the resId given is not null, the visibility of the icon is not View.GONE.
        ImageView iconView = mListView.findViewById(R.id.trailing_icon);
        mListView.setTrailingIcon(
                R.drawable.ntp_customization_bottom_sheet_list_item_background_single);
        assertEquals(View.VISIBLE, iconView.getVisibility());
    }
}
