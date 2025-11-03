// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.view.Gravity;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.widget.Toast;

/**
 * This class is the Java counterpart of ExclusiveAccessBubbleAndroid. It owns the notification
 * toast for the exclusive access APIs (fullscreen, keyboard lock & pointer lock)
 */
@NullMarked
public class ExclusiveAccessBubble {
    private final ExclusiveAccessContext mParentContext;
    private @Nullable Toast mNotificationToast;

    private ExclusiveAccessBubble(ExclusiveAccessContext parentContext) {
        mParentContext = parentContext;
    }

    @CalledByNative
    public static ExclusiveAccessBubble create(ExclusiveAccessContext context) {
        return new ExclusiveAccessBubble(context);
    }

    @CalledByNative
    public void show() {
        if (mNotificationToast != null) {
            mNotificationToast.show();
        }
    }

    @CalledByNative
    public void update(String text) {
        hide();
        mNotificationToast =
                Toast.makeTextWithPriority(
                        mParentContext.getAppContext(),
                        text,
                        Toast.LENGTH_LONG,
                        Toast.ToastPriority.HIGH);
        mNotificationToast.setGravity(Gravity.BOTTOM | Gravity.CENTER, 0, 0);
        show();
    }

    @CalledByNative
    public void hide() {
        if (mNotificationToast != null) {
            mNotificationToast.cancel();
            mNotificationToast = null;
        }
    }

    @CalledByNative
    public boolean isVisible() {
        return mNotificationToast != null;
    }

    @CalledByNative
    public boolean isKeyboardConnected() {
        return DeviceInput.supportsAlphabeticKeyboard();
    }
}
