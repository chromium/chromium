// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.view.Display;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.jni_zero.JniPtr;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;
import org.chromium.ui.mojom.EventConstantsConstants;

@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarControllerFeatureTest {
    private static final long NATIVE_CONTROLLER_BRIDGE = 1234L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkBarControllerFeature.Natives mMockJni;
    @Mock private JniPtr<NativeBrowserWindowInterface> mBrowserWindow;

    @Test
    public void testCreatesAndDestroysController() {
        BookmarkBarControllerFeatureJni.setInstanceForTesting(mMockJni);
        when(mMockJni.createController(mBrowserWindow)).thenReturn(NATIVE_CONTROLLER_BRIDGE);
        var feature = new BookmarkBarControllerFeature();

        feature.onAddedToTask(
                new InitInfo(
                        /* nativeBrowserWindowPtr= */ 0,
                        mBrowserWindow,
                        /* isVisible= */ true,
                        new Rect(),
                        new Rect(),
                        Display.DEFAULT_DISPLAY));
        feature.onFeatureRemoved();

        verify(mMockJni).destroyController(NATIVE_CONTROLLER_BRIDGE);
    }

    @Test
    public void testToEventFlags() {
        assertEquals(
                EventConstantsConstants.EVENT_FLAG_NONE,
                BookmarkBarControllerFeature.toEventFlags(
                        /* metaState= */ 0, /* buttonState= */ 0));
        assertEquals(
                EventConstantsConstants.EVENT_FLAG_CONTROL_DOWN
                        | EventConstantsConstants.EVENT_FLAG_SHIFT_DOWN,
                BookmarkBarControllerFeature.toEventFlags(
                        KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON, /* buttonState= */ 0));
        assertEquals(
                EventConstantsConstants.EVENT_FLAG_ALT_DOWN
                        | EventConstantsConstants.EVENT_FLAG_COMMAND_DOWN,
                BookmarkBarControllerFeature.toEventFlags(
                        KeyEvent.META_ALT_ON | KeyEvent.META_META_ON, /* buttonState= */ 0));
        assertEquals(
                EventConstantsConstants.EVENT_FLAG_MIDDLE_MOUSE_BUTTON,
                BookmarkBarControllerFeature.toEventFlags(
                        /* metaState= */ 0, MotionEvent.BUTTON_TERTIARY));
    }
}
