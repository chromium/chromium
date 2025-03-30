// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.widget.CompoundButton;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/** Unit tests for {@link NtpCardsListItemView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCardsListItemViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MaterialSwitchWithText mMaterialSwitchWithText;

    private NtpCardsListItemView mListView;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mListView = new NtpCardsListItemView(context, null);
        mListView.setMaterialSwitchWithTextForTesting(mMaterialSwitchWithText);
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        String text = "New tab page cards";
        mListView.setTitle(text);
        verify(mMaterialSwitchWithText).setText(text);
    }

    @Test
    @SmallTest
    public void testSetChecked() {
        mListView.setChecked(false);
        verify(mMaterialSwitchWithText).setChecked(eq(false));

        mListView.setChecked(true);
        verify(mMaterialSwitchWithText).setChecked(eq(true));
    }

    @Test
    @SmallTest
    public void testSetOnCheckedChangeListener() {
        CompoundButton.OnCheckedChangeListener listener =
                mock(CompoundButton.OnCheckedChangeListener.class);
        mListView.setOnCheckedChangeListener(listener);
        verify(mMaterialSwitchWithText).setOnCheckedChangeListener(eq(listener));
    }
}
