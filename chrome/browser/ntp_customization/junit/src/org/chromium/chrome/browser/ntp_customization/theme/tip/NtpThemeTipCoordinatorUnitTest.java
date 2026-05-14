// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.tip;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_TIP;

import android.content.Context;
import android.view.ContextThemeWrapper;
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
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link NtpThemeTipCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeTipCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private Runnable mDismissBottomSheet;

    private NtpThemeTipCoordinator mCoordinator;
    private Context mContext;
    private PropertyModel mModel;
    private View mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new NtpThemeTipCoordinator(
                        mContext, mBottomSheetDelegate, mOnClickListener, mDismissBottomSheet);
        mModel = mCoordinator.getPropertyModelForTesting();
        mView = mCoordinator.getViewForTesting();
    }

    @Test
    public void testConstructor() {
        verify(mBottomSheetDelegate).registerBottomSheetLayout(eq(THEME_TIP), eq(mView));
        assertNotNull(mModel.get(NtpThemeTipProperties.CANCEL_BUTTON_CLICK_LISTENER));
        assertNotNull(mModel.get(NtpThemeTipProperties.CUSTOMIZE_BUTTON_CLICK_LISTENER));
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();
        assertNull(mModel.get(NtpThemeTipProperties.CANCEL_BUTTON_CLICK_LISTENER));
        assertNull(mModel.get(NtpThemeTipProperties.CUSTOMIZE_BUTTON_CLICK_LISTENER));
    }

    @Test
    public void testCustomizeButton() {
        View customizeButton = mView.findViewById(R.id.customize_button);
        assertNotNull(customizeButton);
        customizeButton.performClick();
        verify(mOnClickListener).onClick(eq(customizeButton));
    }

    @Test
    public void testCancelButton() {
        View cancelButton = mView.findViewById(R.id.cancel_button);
        assertNotNull(cancelButton);
        cancelButton.performClick();
        verify(mDismissBottomSheet).run();
    }
}
