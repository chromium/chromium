// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_MVT_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER;

import android.view.View;
import android.widget.CompoundButton;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MvtSettingsMediator} */
@RunWith(BaseRobolectricTestRunner.class)
public class MvtSettingsMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mDelegate;
    @Mock View mView;
    @Mock private PropertyModel mBottomSheetPropertyModel;
    @Captor private ArgumentCaptor<View.OnClickListener> mBackPressHandlerCaptor;

    private MvtSettingsMediator mMediator;

    @Before
    public void setUp() {
        mMediator = new MvtSettingsMediator(mBottomSheetPropertyModel, mDelegate);
    }

    @Test
    public void testConstructor() {
        verify(mBottomSheetPropertyModel).set(eq(IS_MVT_SWITCH_CHECKED), anyBoolean());
        verify(mBottomSheetPropertyModel)
                .set(
                        eq(MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER),
                        any(CompoundButton.OnCheckedChangeListener.class));
    }

    @Test
    public void testBackPressHandler() {
        // Verifies that when the mvt settings bottom sheet should show alone, the back press
        // handler should be set to null.
        when(mDelegate.shouldShowAlone()).thenReturn(true);
        new MvtSettingsMediator(mBottomSheetPropertyModel, mDelegate);
        verify(mBottomSheetPropertyModel).set(BACK_PRESS_HANDLER, null);

        // Verifies that when the feed settings bottom sheet is part of the navigation flow starting
        // from the main bottom sheet, and the back press handler should be set to
        // backPressOnCurrentBottomSheet()
        clearInvocations(mBottomSheetPropertyModel);
        when(mDelegate.shouldShowAlone()).thenReturn(false);
        new MvtSettingsMediator(mBottomSheetPropertyModel, mDelegate);
        verify(mBottomSheetPropertyModel)
                .set(eq(BACK_PRESS_HANDLER), mBackPressHandlerCaptor.capture());
        mBackPressHandlerCaptor.getValue().onClick(mView);
        verify(mDelegate).backPressOnCurrentBottomSheet();
    }

    @Test
    public void testOnMvtSwitchToggledAndState() {
        String histogramName = "NewTabPage.Customization.MvtEnabled";

        NtpCustomizationConfigManager configManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, /* value= */ true);
        mMediator.onMvtSwitchToggled(/* isEnabled= */ true);
        assertTrue(configManager.getPrefIsMvtToggleOn());
        assertTrue(mMediator.isMvtTurnedOn());
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, /* value= */ false);
        mMediator.onMvtSwitchToggled(/* isEnabled= */ false);
        assertFalse(configManager.getPrefIsMvtToggleOn());
        assertFalse(mMediator.isMvtTurnedOn());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mBottomSheetPropertyModel).set(eq(BACK_PRESS_HANDLER), eq(null));
        verify(mBottomSheetPropertyModel).set(eq(MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER), eq(null));
    }
}
