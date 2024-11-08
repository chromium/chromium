// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
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
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link AcknowledgeGroupedCredentialSheetController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class AcknowledgeGroupedCredentialSheetModuleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private AcknowledgeGroupedCredentialSheetBridge.Natives mBridgeJniMock;
    private AcknowledgeGroupedCredentialSheetBridge mBridge;
    private WindowAndroid mWindowAndroid;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    private final ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);
    private final ArgumentCaptor<BottomSheetContent> mBottomSheetContentCaptor =
            ArgumentCaptor.forClass(BottomSheetContent.class);
    private static final long TEST_NATIVE_POINTER = 1;
    private static final String CURRENT_DOMAIN = "current.com";
    private static final String CREDENTIAL_DOMAIN = "credential.com";

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mJniMocker.mock(AcknowledgeGroupedCredentialSheetBridgeJni.TEST_HOOKS, mBridgeJniMock);
        mWindowAndroid = new WindowAndroid(ContextUtils.getApplicationContext());
        setUpBottomSheetController();
        mBridge = new AcknowledgeGroupedCredentialSheetBridge(TEST_NATIVE_POINTER, mWindowAndroid);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindowAndroid.destroy();
    }

    private void setUpBottomSheetController() {
        BottomSheetControllerFactory.attach(mWindowAndroid, mBottomSheetController);
        when(mBottomSheetController.requestShowContent(
                        mBottomSheetContentCaptor.capture(), anyBoolean()))
                .thenReturn(true);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
    }

    @Test
    public void showsAndHidesBottomSheet() {
        mBridge.show(CURRENT_DOMAIN, CREDENTIAL_DOMAIN);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mBottomSheetController).addObserver(any());

        when(mBottomSheetController.getCurrentSheetContent())
                .thenReturn(mBottomSheetContentCaptor.getValue());

        mBottomSheetObserverCaptor
                .getValue()
                .onSheetClosed(BottomSheetController.StateChangeReason.SWIPE);
        verify(mBottomSheetController).removeObserver(mBottomSheetObserverCaptor.getValue());
        verify(mBridgeJniMock).onDismissed(TEST_NATIVE_POINTER, false);
    }

    @Test
    public void testAcceptButtonClicked() {
        mBridge.show(CURRENT_DOMAIN, CREDENTIAL_DOMAIN);

        doAnswer(
                        (invocation) -> {
                            mBottomSheetObserverCaptor
                                    .getValue()
                                    .onSheetClosed(BottomSheetController.StateChangeReason.NONE);
                            return null;
                        })
                .when(mBottomSheetController)
                .hideContent(any(), anyBoolean());
        when(mBottomSheetController.getCurrentSheetContent())
                .thenReturn(mBottomSheetContentCaptor.getValue());

        mBottomSheetContentCaptor
                .getValue()
                .getContentView()
                .findViewById(R.id.confirmation_button)
                .callOnClick();
        verify(mBridgeJniMock).onDismissed(TEST_NATIVE_POINTER, true);
        verify(mBottomSheetController).hideContent(mBottomSheetContentCaptor.getValue(), true);
    }

    @Test
    public void testNegativeButtonClicked() {
        mBridge.show(CURRENT_DOMAIN, CREDENTIAL_DOMAIN);

        mBottomSheetContentCaptor
                .getValue()
                .getContentView()
                .findViewById(R.id.cancel_button)
                .callOnClick();
        verify(mBridgeJniMock).onDismissed(TEST_NATIVE_POINTER, false);
        verify(mBottomSheetController).hideContent(mBottomSheetContentCaptor.getValue(), true);
    }
}
