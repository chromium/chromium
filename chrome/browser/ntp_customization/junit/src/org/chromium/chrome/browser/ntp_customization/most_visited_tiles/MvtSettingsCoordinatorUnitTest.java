// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_MVT_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.ViewFlipper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link MvtSettingsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class MvtSettingsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock BottomSheetDelegate mBottomSheetDelegate;
    private MvtSettingsCoordinator mCoordinator;
    private Context mContext;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        org.chromium.chrome.browser.ntp_customization.R.style
                                .Theme_BrowserUI_DayNight);
        mCoordinator = new MvtSettingsCoordinator(mContext, mBottomSheetDelegate);
        mPropertyModel = new PropertyModel(BOTTOM_SHEET_KEYS);
    }

    @Test
    public void testConstructor() {
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(
                        eq(NtpCustomizationCoordinator.BottomSheetType.MVT), any(View.class));
        assertNotNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    public void testBindMvtSettingsBottomSheet() {
        // Adds the mvt bottom sheet to the view of the activity.
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        ViewFlipper viewFlipperView = contentView.findViewById(R.id.ntp_customization_view_flipper);
        View mvtBottomSheet =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_mvt_bottom_sheet, viewFlipperView, true);
        PropertyModelChangeProcessor.create(
                mPropertyModel, mvtBottomSheet, BottomSheetViewBinder::bind);

        // Verifies the on checked change listener is added to the mvt bottom sheet's mvt switch.
        CompoundButton.OnCheckedChangeListener onCheckedChangeListener =
                mock(CompoundButton.OnCheckedChangeListener.class);
        mPropertyModel.set(MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER, onCheckedChangeListener);
        MaterialSwitchWithText mvtSwitch = mvtBottomSheet.findViewById(R.id.mvt_switch_button);
        mvtSwitch.setChecked(true);
        verify(onCheckedChangeListener)
                .onCheckedChanged(eq(mvtBottomSheet.findViewById(R.id.switch_widget)), eq(true));
        mvtSwitch.setChecked(false);
        verify(onCheckedChangeListener)
                .onCheckedChanged(eq(mvtBottomSheet.findViewById(R.id.switch_widget)), eq(false));

        // Verifies the mvt switch will get updated timely.
        assertFalse(mvtSwitch.isChecked());
        mPropertyModel.set(IS_MVT_SWITCH_CHECKED, true);
        assertTrue(mvtSwitch.isChecked());
        mPropertyModel.set(IS_MVT_SWITCH_CHECKED, false);
        assertFalse(mvtSwitch.isChecked());
    }

    @Test
    public void testDestroy() {
        MvtSettingsMediator mediator = mock(MvtSettingsMediator.class);
        mCoordinator.setMediatorForTesting(mediator);
        mCoordinator.destroy();
        verify(mediator).destroy();
    }
}
