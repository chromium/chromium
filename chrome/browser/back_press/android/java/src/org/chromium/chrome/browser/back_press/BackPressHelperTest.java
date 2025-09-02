// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

import java.util.concurrent.atomic.AtomicBoolean;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
public class BackPressHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private BackPressHelper.OnKeyDownHandler mOnKeyDownHandler;

    private OnBackPressedDispatcher mDispatcher;

    @Mock private BackPressHandler mMockHandler;
    @Mock private ObservableSupplier<Boolean> mMockHandleBackPressChangedSupplier;
    @Mock private KeyEvent mMockKeyEvent;
    @Mock private LifecycleOwner mLifecycleOwner;
    @Mock private Lifecycle mMockLifecycle;

    @Before
    public void setUp() {
        mDispatcher = new OnBackPressedDispatcher();
        when(mLifecycleOwner.getLifecycle()).thenReturn(mMockLifecycle);

        when(mMockHandler.getHandleBackPressChangedSupplier())
                .thenReturn(mMockHandleBackPressChangedSupplier);
        when(mMockHandleBackPressChangedSupplier.get()).thenReturn(true);

        when(mMockKeyEvent.getRepeatCount()).thenReturn(0);
        when(mMockKeyEvent.isShiftPressed()).thenReturn(false);
        when(mMockKeyEvent.isCtrlPressed()).thenReturn(false);
        when(mMockKeyEvent.isAltPressed()).thenReturn(false);
        when(mMockKeyEvent.isMetaPressed()).thenReturn(false);

        mOnKeyDownHandler = BackPressHelper.create(mLifecycleOwner, mDispatcher, mMockHandler);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
    public void onKeyDown_whenFeatureDisabled_returnsFalse() {
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
        verify(mMockHandler, never()).handleEscPress();
    }

    @Test
    public void onKeyDown_withNonEscapeKey_returnsFalseAndDoesNothing() {
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ENTER, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
    }

    @Test
    public void onKeyDown_whenHandlerIsNotActive_returnsFalse() {
        when(mMockHandleBackPressChangedSupplier.get()).thenReturn(false);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
    }

    @Test
    public void onKeyDown_whenHandlerIsActive_andInvokesBackAction_triggersDispatcher() {
        final AtomicBoolean backPressedCalled = new AtomicBoolean(false);
        mDispatcher.addCallback(
                new OnBackPressedCallback(true) {
                    @Override
                    public void handleOnBackPressed() {
                        backPressedCalled.set(true);
                    }
                });
        when(mMockHandler.invokeBackActionOnEscape()).thenReturn(true);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertTrue(result);
        assertTrue(backPressedCalled.get());
        verify(mMockHandler, never()).handleEscPress();
    }

    @Test
    public void onKeyDown_whenHandlerIsActive_andHandlesCustomAction_succeeds() {
        when(mMockHandler.invokeBackActionOnEscape()).thenReturn(false);
        when(mMockHandler.handleEscPress()).thenReturn(true);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertTrue(result);
        verify(mMockHandler).handleEscPress();
    }

    @Test
    public void onKeyDown_whenHandlerIsActive_andHandlesCustomAction_fails() {
        when(mMockHandler.invokeBackActionOnEscape()).thenReturn(false);
        when(mMockHandler.handleEscPress()).thenReturn(false);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler).handleEscPress();
    }

    @Test
    public void onKeyDown_whenHandlerIsActive_andHandlesCustomAction_returnsNull() {
        when(mMockHandler.invokeBackActionOnEscape()).thenReturn(false);
        when(mMockHandler.handleEscPress()).thenReturn(null);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler).handleEscPress();
    }

    @Test
    public void onKeyDown_withRepeatedEscapeKey_returnsFalse() {
        when(mMockKeyEvent.getRepeatCount()).thenReturn(1);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
    }

    @Test
    public void onKeyDown_withEscapeAndShiftKey_returnsFalse() {
        when(mMockKeyEvent.isShiftPressed()).thenReturn(true);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
    }

    @Test
    public void onKeyDown_withEscapeAndCtrlKey_returnsFalse() {
        when(mMockKeyEvent.isCtrlPressed()).thenReturn(true);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
    }

    @Test
    public void onKeyDown_withEscapeAndAltKey_returnsFalse() {
        when(mMockKeyEvent.isAltPressed()).thenReturn(true);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
    }

    @Test
    public void onKeyDown_withEscapeAndMetaKey_returnsFalse() {
        when(mMockKeyEvent.isMetaPressed()).thenReturn(true);
        boolean result = mOnKeyDownHandler.onKeyDown(KeyEvent.KEYCODE_ESCAPE, mMockKeyEvent);
        assertFalse(result);
        verify(mMockHandler, never()).invokeBackActionOnEscape();
    }
}
