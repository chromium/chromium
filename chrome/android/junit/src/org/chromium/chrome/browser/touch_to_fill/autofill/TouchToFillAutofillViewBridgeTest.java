// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link TouchToFillAutofillViewBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TouchToFillAutofillViewBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TouchToFillAutofillViewBridge.Natives mNativeMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;

    @Mock private WebContents mWebContents;
    private ViewAndroidDelegate mViewAndroidDelegate;
    @Mock private ViewGroup mContainerView;

    private static final long NATIVE_POINTER = 123456L;
    private TouchToFillAutofillViewBridge mBridge;

    @Before
    public void setUp() {
        TouchToFillAutofillViewBridgeJni.setInstanceForTesting(mNativeMock);

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mViewAndroidDelegate = ViewAndroidDelegate.createBasicDelegate(mContainerView);

        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardDelegate);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(mViewAndroidDelegate);
        when(mContainerView.isFocused()).thenReturn(false);

        mBridge =
                new TouchToFillAutofillViewBridge(
                        NATIVE_POINTER,
                        activity,
                        mBottomSheetController,
                        mWindowAndroid,
                        mWebContents);
    }

    @Test
    public void testShow_RegistersObserver() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        assertNotNull(captor.getAllValues().get(0));
    }

    @Test
    public void testDestroy_RemovesObserver() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        BottomSheetObserver bridgeObserver = captor.getAllValues().get(0);

        mBridge.destroy();
        verify(mBottomSheetController).removeObserver(bridgeObserver);
    }

    @Test
    public void testShow_RegistersObserverOnlyOnce() {
        mBridge.show();
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());

        BottomSheetObserver bridgeObserver = captor.getAllValues().get(0);
        int count = 0;
        for (BottomSheetObserver o : captor.getAllValues()) {
            if (o == bridgeObserver) {
                count++;
            }
        }
        assertEquals(1, count);
    }

    @Test
    public void testDestroy_DoesNotRemoveObserverIfNotRegistered() {
        mBridge.destroy();
        verify(mBottomSheetController, times(1)).removeObserver(any());
    }

    @Test
    public void testOnSheetClosed_NotifiesNative() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        BottomSheetObserver observer = captor.getAllValues().get(0);

        observer.onSheetClosed(BottomSheetController.StateChangeReason.BACK_PRESS);

        verify(mNativeMock).onDismissed(NATIVE_POINTER);
    }

    @Test
    public void testOnSheetClosed_HandlesDestroyedBridgeGracefully() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        BottomSheetObserver observer = captor.getAllValues().get(0);

        mBridge.destroy();
        observer.onSheetClosed(BottomSheetController.StateChangeReason.BACK_PRESS);

        verifyNoInteractions(mNativeMock);
    }

    @Test
    public void testOnSheetClosed_TriggersKeyboardRestore() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        BottomSheetObserver observer = captor.getAllValues().get(0);

        observer.onSheetClosed(BottomSheetController.StateChangeReason.BACK_PRESS);

        verify(mContainerView).requestFocus();
        verify(mKeyboardDelegate).showKeyboard(mContainerView);
        verify(mBottomSheetController).removeObserver(observer);
    }

    @Test
    public void testOnSheetClosed_RestoresKeyboardBeforeNotifyingNative() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        BottomSheetObserver observer = captor.getAllValues().get(0);

        observer.onSheetClosed(BottomSheetController.StateChangeReason.BACK_PRESS);

        InOrder inOrder = inOrder(mContainerView, mKeyboardDelegate, mNativeMock);
        inOrder.verify(mContainerView).requestFocus();
        inOrder.verify(mKeyboardDelegate).showKeyboard(mContainerView);
        inOrder.verify(mNativeMock).onDismissed(NATIVE_POINTER);
    }

    @Test
    public void testOnSheetClosed_DoesNotRestoreIfOmniboxFocused() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        BottomSheetObserver observer = captor.getAllValues().get(0);

        observer.onSheetClosed(BottomSheetController.StateChangeReason.OMNIBOX_FOCUS);

        verifyNoInteractions(mKeyboardDelegate);
        verify(mContainerView, never()).requestFocus();
        verify(mBottomSheetController).removeObserver(observer);
    }

    @Test
    public void testOnSheetClosed_DoesNotRestoreIfWebContentsDestroyed() {
        mBridge.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, atLeastOnce()).addObserver(captor.capture());
        BottomSheetObserver observer = captor.getAllValues().get(0);

        when(mWebContents.isDestroyed()).thenReturn(true);

        observer.onSheetClosed(BottomSheetController.StateChangeReason.BACK_PRESS);

        verifyNoInteractions(mKeyboardDelegate);
        verify(mBottomSheetController).removeObserver(observer);
    }
}
