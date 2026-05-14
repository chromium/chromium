// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.tip;

import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NtpThemeTipViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeTipViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mCancelClickListener;
    @Mock private View.OnClickListener mCustomizeClickListener;

    private PropertyModel mModel;
    private View mView;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.ntp_customization_theme_tip_bottom_sheet_layout,
                                null,
                                false);
        mModel = new PropertyModel(NtpThemeTipProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, NtpThemeTipViewBinder::bind);
    }

    @Test
    public void testCancelButtonClickListener() {
        mModel.set(NtpThemeTipProperties.CANCEL_BUTTON_CLICK_LISTENER, mCancelClickListener);
        View cancelButton = mView.findViewById(R.id.cancel_button);
        cancelButton.performClick();
        verify(mCancelClickListener).onClick(cancelButton);
    }

    @Test
    public void testCustomizeButtonClickListener() {
        mModel.set(NtpThemeTipProperties.CUSTOMIZE_BUTTON_CLICK_LISTENER, mCustomizeClickListener);
        View customizeButton = mView.findViewById(R.id.customize_button);
        customizeButton.performClick();
        verify(mCustomizeClickListener).onClick(customizeButton);
    }
}
