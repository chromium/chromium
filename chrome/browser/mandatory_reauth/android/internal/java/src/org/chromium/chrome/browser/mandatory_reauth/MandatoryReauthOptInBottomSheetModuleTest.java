// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Tests for {@link MandatoryReauthOptInBottomSheetViewBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class MandatoryReauthOptInBottomSheetModuleTest {
    private MandatoryReauthOptInBottomSheetViewBridge mBridge;
    private MandatoryReauthOptInBottomSheetComponent mComponent;
    private final ArgumentCaptor<BottomSheetObserver> mObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private BottomSheetController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        setUpBottomSheetController();
        mComponent = new MandatoryReauthOptInBottomSheetCoordinator(
                Robolectric.buildActivity(Activity.class).get(), mController);
        mBridge = new MandatoryReauthOptInBottomSheetViewBridge(mComponent);
    }

    private void setUpBottomSheetController() {
        when(mController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mController).addObserver(mObserverCaptor.capture());
    }

    @Test
    public void testShowsBottomSheet() {
        mBridge.show();
        verify(mController).requestShowContent(any(), anyBoolean());
        verify(mController).addObserver(any());
    }

    @Test
    public void testClosesBottomSheet() {
        mBridge.show();

        mBridge.close();
        verify(mController).hideContent(any(), anyBoolean());
        verify(mController).removeObserver(mObserverCaptor.getValue());
    }
}
