// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** This class allows native code to discover the root view of the current Window. */
@JNINamespace("base::android")
public class InputHintChecker {

    /**
     * Sets the current View for the next input hint requests from native.
     *
     * <p>Stores/replaces the global weak reference to the root view. To be effective, the View
     * instance must have a ViewRootImpl attached to it (internal class in Android Framework). An
     * example of such a "root view" can be obtained via `Activity.getWindow().getDecorView()`, and
     * only after {@link android.app.Activity#setContentView(android.view.View)}.
     *
     * @param view The View to pull the input hint from next time.
     */
    public static void setView(@Nullable View view) {
        if (Build.VERSION.SDK_INT >= VERSION_CODES.UPSIDE_DOWN_CAKE) {
            // TODO(pasko): Restrict to SDK_INT >= V when V is available in VERSION_CODES.
            InputHintCheckerJni.get().setView(view);
        }
    }

    @NativeMethods
    interface Natives {
        void setView(View view);
    }
}
