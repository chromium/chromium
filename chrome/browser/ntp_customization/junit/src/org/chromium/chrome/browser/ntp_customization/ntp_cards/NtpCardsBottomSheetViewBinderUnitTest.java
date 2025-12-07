// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.MaterialSwitchWithTextListContainerView;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NtpCardsBottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCardsBottomSheetViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mParentView;
    @Mock private MaterialSwitchWithText mMaterialSwitch;
    @Mock private OnCheckedChangeListener mListener;

    private Context mContext;
    private PropertyModel mPropertyModel;
    private MaterialSwitchWithText mAllCardsSwitch;
    private MaterialSwitchWithTextListContainerView mMaterialSwitchWithTextListContainerView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        mMaterialSwitchWithTextListContainerView =
                spy(new MaterialSwitchWithTextListContainerView(mContext, /* attrs= */ null));
        mAllCardsSwitch = spy(new MaterialSwitchWithText(mContext, /* attrs= */ null));

        when(mParentView.findViewById(R.id.cards_switch_button)).thenReturn(mAllCardsSwitch);
        when(mParentView.findViewById(R.id.ntp_cards_container))
                .thenReturn(mMaterialSwitchWithTextListContainerView);
        when(mMaterialSwitchWithTextListContainerView.getChildCount()).thenReturn(1);
        when(mMaterialSwitchWithTextListContainerView.getChildAt(0)).thenReturn(mMaterialSwitch);

        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.NTP_CARD_SETTINGS_KEYS);
        PropertyModelChangeProcessor.create(
                mPropertyModel, mParentView, NtpCardsBottomSheetViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testBindAllCardsSwitchListener() {
        mPropertyModel.set(
                NtpCustomizationViewProperties.ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                mListener);
        mAllCardsSwitch.performClick();
        verify(mListener).onCheckedChanged(any(), eq(true));
    }

    @Test
    @SmallTest
    public void testBindAreCardSwitchesEnabled() {
        mPropertyModel.set(NtpCustomizationViewProperties.ARE_CARD_SWITCHES_ENABLED, false);
        verify(mAllCardsSwitch).setChecked(false);
        verify(mMaterialSwitchWithTextListContainerView).setAllModuleSwitchesEnabled(false);
        verify(mMaterialSwitch).setEnabled(false);

        mPropertyModel.set(NtpCustomizationViewProperties.ARE_CARD_SWITCHES_ENABLED, true);
        verify(mAllCardsSwitch).setChecked(true);
        verify(mMaterialSwitchWithTextListContainerView).setAllModuleSwitchesEnabled(true);
        verify(mMaterialSwitch).setEnabled(true);
    }
}
