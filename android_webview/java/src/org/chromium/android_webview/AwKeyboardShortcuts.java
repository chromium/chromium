// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.KeyEvent;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.content_public.browser.KeyboardShortcutRecorder;
import org.chromium.content_public.browser.KeyboardShortcutRecorder.KeyboardShortcut;

/**
 * Handles WebView keyboard shortcut events that weren't handled in {@link
 * AwWebContentsDelegateAdapter#handleKeyboardEvent(KeyEvent)}.
 */
/**
 * TODO(wbjacksonjr) Possibly merge this class with {@link
 * org.chromium.chrome.browser.KeyboardShortcuts}
 */
public class AwKeyboardShortcuts {
    private static final int CTRL = 1 << 31;
    private static final int ALT = 1 << 30;
    private static final int SHIFT = 1 << 29;

    private AwKeyboardShortcuts() {}

    private static int getMetaState(KeyEvent event) {
        return (event.isCtrlPressed() ? CTRL : 0)
                | (event.isAltPressed() ? ALT : 0)
                | (event.isShiftPressed() ? SHIFT : 0);
    }

    public static boolean onKeyDown(KeyEvent event, AwContents awContents) {
        int keyCode = event.getKeyCode();
        if (event.getRepeatCount() != 0
                || event.getAction() != KeyEvent.ACTION_DOWN
                || KeyEvent.isModifierKey(keyCode)) {
            return false;
        }

        int metaState = getMetaState(event);
        int keyCodeAndMeta = keyCode | metaState;

        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_ZOOM_KEYBOARD_SHORTCUTS)) {
            return handleZoomShortcut(awContents, keyCodeAndMeta);
        }
        return false;
    }

    private static boolean handleZoomShortcut(AwContents awContents, int keyCodeAndMeta) {
        // We want to return true even if zoom is not supported as technically the keyboard shortcut
        // was handled
        boolean supportsZoom = awContents.getSettings().supportZoom();
        switch (keyCodeAndMeta) {
            case CTRL | KeyEvent.KEYCODE_PLUS:
            case CTRL | KeyEvent.KEYCODE_EQUALS:
            case CTRL | SHIFT | KeyEvent.KEYCODE_PLUS:
            case CTRL | SHIFT | KeyEvent.KEYCODE_EQUALS:
            case KeyEvent.KEYCODE_ZOOM_IN:
                if (supportsZoom) {
                    awContents.zoomIn();
                    KeyboardShortcutRecorder.recordKeyboardShortcut(KeyboardShortcut.ZOOM_IN);
                }
                return true;
            case CTRL | KeyEvent.KEYCODE_MINUS:
            case KeyEvent.KEYCODE_ZOOM_OUT:
                if (supportsZoom) {
                    awContents.zoomOut();
                    KeyboardShortcutRecorder.recordKeyboardShortcut(KeyboardShortcut.ZOOM_OUT);
                }
                return true;
            case CTRL | KeyEvent.KEYCODE_0:
                if (supportsZoom) {
                    awContents.zoomReset();
                    KeyboardShortcutRecorder.recordKeyboardShortcut(KeyboardShortcut.ZOOM_RESET);
                }
                return true;
        }
        return false;
    }
}
