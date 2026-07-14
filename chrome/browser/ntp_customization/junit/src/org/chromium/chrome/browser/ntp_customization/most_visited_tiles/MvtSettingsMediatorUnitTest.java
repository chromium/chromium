// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.junit.Assert.assertEquals;
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

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MvtSettingsMediator} */
@RunWith(BaseRobolectricTestRunner.class)
public class MvtSettingsMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mDelegate;
    @Mock View mView;
    @Mock private PropertyModel mBottomSheetPropertyModel;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Captor private ArgumentCaptor<View.OnClickListener> mBackPressHandlerCaptor;

    private MvtSettingsMediator mMediator;
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        UserPrefs.setPrefServiceForTesting(mPrefService);
        mProfileSupplier.set(mProfile);
        mMediator = new MvtSettingsMediator(mBottomSheetPropertyModel, mDelegate, mProfileSupplier);
    }

    @After
    public void tearDown() {
        UserPrefs.setPrefServiceForTesting(null);
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
        new MvtSettingsMediator(mBottomSheetPropertyModel, mDelegate, mProfileSupplier);
        verify(mBottomSheetPropertyModel).set(BACK_PRESS_HANDLER, null);

        // Verifies that when the feed settings bottom sheet is part of the navigation flow starting
        // from the main bottom sheet, and the back press handler should be set to
        // backPressOnCurrentBottomSheet()
        clearInvocations(mBottomSheetPropertyModel);
        when(mDelegate.shouldShowAlone()).thenReturn(false);
        new MvtSettingsMediator(mBottomSheetPropertyModel, mDelegate, mProfileSupplier);
        verify(mBottomSheetPropertyModel)
                .set(eq(BACK_PRESS_HANDLER), mBackPressHandlerCaptor.capture());
        mBackPressHandlerCaptor.getValue().onClick(mView);
        verify(mDelegate).backPressOnCurrentBottomSheet();
    }

    @Test
    public void testOnMvtSwitchToggled_Enabled() {
        testOnMvtSwitchToggledImpl(/* isEnabled= */ true);
    }

    @Test
    public void testOnMvtSwitchToggled_Disabled() {
        testOnMvtSwitchToggledImpl(/* isEnabled= */ false);
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mBottomSheetPropertyModel).set(eq(BACK_PRESS_HANDLER), eq(null));
        verify(mBottomSheetPropertyModel).set(eq(MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER), eq(null));
    }

    private void testOnMvtSwitchToggledImpl(boolean isEnabled) {
        String histogramName = "NewTabPage.Customization.MvtEnabled";

        NtpCustomizationConfigManager configManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, isEnabled);
        mMediator.onMvtSwitchToggled(isEnabled);

        assertEquals(isEnabled, configManager.getPrefIsMvtToggleOn());
        assertEquals(isEnabled, mMediator.isMvtTurnedOn());
        verify(mPrefService).setBoolean(Pref.NTP_SHORTCUTS_VISIBLE, isEnabled);
        histogramWatcher.assertExpected();
    }
}
