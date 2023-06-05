// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

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
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Tests for {@link TouchToFillPasswordGenerationBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class TouchToFillPasswordGenerationModuleTest {
    private TouchToFillPasswordGenerationBridge mBridge;
    private final ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private TouchToFillPasswordGenerationBridge.Natives mBridgeJniMock;

    private static final long sDummyNativePointer = 1;
    private static final String sTestEmailAddress = "test@email.com";
    private static final String sGeneratedPassword = "Strong generated password";

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mJniMocker.mock(TouchToFillPasswordGenerationBridgeJni.TEST_HOOKS, mBridgeJniMock);
        setUpBottomSheetController();
        mBridge = new TouchToFillPasswordGenerationBridge(sDummyNativePointer,
                Robolectric.buildActivity(Activity.class).get(), mBottomSheetController);
    }

    private void setUpBottomSheetController() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
    }

    @Test
    public void showsAndHidesBottomSheet() {
        mBridge.show(sGeneratedPassword, sTestEmailAddress);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mBottomSheetController).addObserver(any());

        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.SWIPE);
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
        verify(mBottomSheetController).removeObserver(mBottomSheetObserverCaptor.getValue());
    }

    @Test
    public void testBottomSheetForceHide() {
        mBridge.show(sGeneratedPassword, sTestEmailAddress);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());

        mBridge.hide();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
        verify(mBridgeJniMock).onDismissed(sDummyNativePointer);
    }
}
