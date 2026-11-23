// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetBridge.NativeAutofillAiAttributionVerificationSheetBridge;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AutofillAiAttributionVerificationSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillAiAttributionVerificationSheetBridgeTest {
    private static final long NATIVE_BRIDGE = 12345L;
    private static final long OTHER_NATIVE_BRIDGE = 67890L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private AutofillAiAttributionVerificationSheetBridge.Natives mBridgeNatives;

    private TestActivity mActivity;
    private WindowAndroid mWindow;
    private JniRawPtr<NativeAutofillAiAttributionVerificationSheetBridge> mNativeBridge;
    private AutofillAiAttributionVerificationSheetBridge mBridge;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mWindow = new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ true);
        lenient()
                .when(mBottomSheetController.requestShowContent(any(), anyBoolean()))
                .thenReturn(true);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        AutofillAiAttributionVerificationSheetBridgeJni.setInstanceForTesting(mBridgeNatives);
        mNativeBridge = JniRawPtr.createForTesting(NATIVE_BRIDGE);
        mBridge = new AutofillAiAttributionVerificationSheetBridge(mNativeBridge, mWindow);
    }

    @After
    public void tearDown() {
        mBridge.destroy();
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testRequestShowContent_showsAndExpandsSheet() {
        mBridge.requestShowContent();

        AutofillAiAttributionVerificationSheetCoordinator coordinator =
                mBridge.getCoordinatorForTesting();
        assertNotNull(coordinator);
        verify(mBottomSheetController)
                .requestShowContent(eq(coordinator.getViewForTesting()), /* animate= */ eq(true));
        verify(mBottomSheetController).expandSheet();
    }

    @Test
    public void testRequestShowContent_calledMultipleTimes_destroysPreviousCoordinator() {
        mBridge.requestShowContent();
        AutofillAiAttributionVerificationSheetCoordinator firstCoordinator =
                mBridge.getCoordinatorForTesting();
        assertNotNull(firstCoordinator);

        mBridge.requestShowContent();
        AutofillAiAttributionVerificationSheetCoordinator secondCoordinator =
                mBridge.getCoordinatorForTesting();
        assertNotNull(secondCoordinator);

        verify(mBottomSheetController)
                .hideContent(eq(firstCoordinator.getViewForTesting()), /* animate= */ eq(false));
        verify(mBridgeNatives, never()).onDismissed(any());
    }

    @Test
    public void testRequestShowContent_whenBottomSheetControllerNull_notifiesDismissed() {
        WindowAndroid windowWithoutController =
                new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ true);
        JniRawPtr<NativeAutofillAiAttributionVerificationSheetBridge> nativeBridge =
                JniRawPtr.createForTesting(OTHER_NATIVE_BRIDGE);
        AutofillAiAttributionVerificationSheetBridge bridge =
                new AutofillAiAttributionVerificationSheetBridge(
                        nativeBridge, windowWithoutController);

        bridge.requestShowContent();

        verify(mBridgeNatives).onDismissed(nativeBridge);
        assertNull(bridge.getCoordinatorForTesting());
        bridge.destroy();
        windowWithoutController.destroy();
    }

    @Test
    public void testRequestShowContent_whenContextNull_notifiesDismissed() {
        WindowAndroid window = mock(WindowAndroid.class);
        when(window.getContext()).thenReturn(new WeakReference<>(null));
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        JniRawPtr<NativeAutofillAiAttributionVerificationSheetBridge> nativeBridge =
                JniRawPtr.createForTesting(OTHER_NATIVE_BRIDGE);
        AutofillAiAttributionVerificationSheetBridge bridge =
                new AutofillAiAttributionVerificationSheetBridge(nativeBridge, window);

        bridge.requestShowContent();

        verify(mBridgeNatives).onDismissed(nativeBridge);
        assertNull(bridge.getCoordinatorForTesting());
        bridge.destroy();
    }

    @Test
    public void testOnDismissed_notifiesNativeAndClearsCoordinator() {
        mBridge.requestShowContent();
        assertNotNull(mBridge.getCoordinatorForTesting());

        mBridge.onDismissed();

        assertNull(mBridge.getCoordinatorForTesting());
        verify(mBridgeNatives).onDismissed(mNativeBridge);
    }

    @Test
    public void testDestroy_hidesCoordinatorWithoutNotifyingNative() {
        mBridge.requestShowContent();
        AutofillAiAttributionVerificationSheetCoordinator coordinator =
                mBridge.getCoordinatorForTesting();
        assertNotNull(coordinator);

        mBridge.destroy();

        assertNull(mBridge.getCoordinatorForTesting());
        verify(mBottomSheetController)
                .hideContent(eq(coordinator.getViewForTesting()), /* animate= */ eq(false));
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testDestroy_whenCoordinatorNotCreated_noOp() {
        mBridge.destroy();

        verifyNoInteractions(mBottomSheetController);
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnDismissed_whenNativeDestroyed_doesNothing() {
        mBridge.destroy();
        mBridge.onDismissed();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testRequestShowContent_whenNativeDestroyed_doesNothing() {
        mBridge.destroy();
        mBridge.requestShowContent();

        assertNull(mBridge.getCoordinatorForTesting());
        verifyNoInteractions(mBottomSheetController);
        verifyNoInteractions(mBridgeNatives);
    }
}
