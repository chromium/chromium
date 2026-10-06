// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.FrameLayout;

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

    @Mock private OnCheckedChangeListener mListener;

    private PropertyModel mPropertyModel;
    private MaterialSwitchWithText mAllCardsSwitch;
    private MaterialSwitchWithText mMaterialSwitch;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        MaterialSwitchWithTextListContainerView containerView =
                new MaterialSwitchWithTextListContainerView(context, /* attrs= */ null);
        containerView.setId(R.id.ntp_cards_container);
        mMaterialSwitch = new MaterialSwitchWithText(context, /* attrs= */ null);
        containerView.addView(mMaterialSwitch);
        mAllCardsSwitch = new MaterialSwitchWithText(context, /* attrs= */ null);
        mAllCardsSwitch.setId(R.id.cards_switch_button);

        FrameLayout parentView = new FrameLayout(context);
        parentView.addView(mAllCardsSwitch);
        parentView.addView(containerView);

        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.NTP_CARD_SETTINGS_KEYS);
        PropertyModelChangeProcessor.create(
                mPropertyModel, parentView, NtpCardsBottomSheetViewBinder::bind);
    }

    @Test
    public void testBindAllCardsSwitchListener() {
        mPropertyModel.set(
                NtpCustomizationViewProperties.ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                mListener);
        mAllCardsSwitch.performClick();
        verify(mListener).onCheckedChanged(any(), eq(true));
    }

    @Test
    public void testBindAreCardSwitchesEnabled() {
        mPropertyModel.set(NtpCustomizationViewProperties.ARE_CARD_SWITCHES_ENABLED, true);
        assertTrue(mAllCardsSwitch.isChecked());
        assertTrue(mMaterialSwitch.isEnabled());

        mPropertyModel.set(NtpCustomizationViewProperties.ARE_CARD_SWITCHES_ENABLED, false);
        assertFalse(mAllCardsSwitch.isChecked());
        assertFalse(mMaterialSwitch.isEnabled());
    }
}
