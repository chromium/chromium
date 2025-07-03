// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;

import android.view.View;

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
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
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
    public void testDestroy() {
        mMediator.destroy();
        verify(mBottomSheetPropertyModel).set(BACK_PRESS_HANDLER, null);
    }
}
