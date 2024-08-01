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

    private static boolean sAllowSetViewForTesting;

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
        if (sAllowSetViewForTesting || Build.VERSION.SDK_INT >= VERSION_CODES.VANILLA_ICE_CREAM) {
            InputHintCheckerJni.get().setView(view);
        }
    }

    public static void setAllowSetViewForTesting(boolean allow) {
        sAllowSetViewForTesting = allow;
    }

    /**
     * Returns true iff the asynchronous initialization has completed successfully.
     *
     * <p>This method is not exposed outside of testing because before initialization checking for
     * the hint silently reports that no input is queued.
     */
    public static boolean isInitializedForTesting() {
        return InputHintCheckerJni.get().isInitializedForTesting(); // IN-TEST
    }

    public static boolean failedToInitializeForTesting() {
        return InputHintCheckerJni.get().failedToInitializeForTesting(); // IN-TEST
    }

    /**
     * Returns the result of calling view.probablyHasInput() on the View that was set before with
     * setView().
     *
     * <p>This method is not exposed outside of testing because the intention is to only invoke it
     * from native.
     */
    public static boolean hasInputForTesting() {
        return InputHintCheckerJni.get().hasInputForTesting(); // IN-TEST
    }

    public static boolean hasInputWithThrottlingForTesting() {
        return InputHintCheckerJni.get().hasInputWithThrottlingForTesting(); // IN-TEST
    }

    /**
     * Set the initial view incorrectly to cause initialization failure.
     *
     * <p>Wrongly initialized InputHintChecker should not be used across tests, hence tests using
     * this method cannot be batched. Therefore, there is no need to reset this state between tests.
     */
    public static void setWrongViewForTesting() {
        // On Android V all instances of View have the method probablyHasInput(). Use an
        // instance of another class to reliably fail at finding this method on all OS releases.
        InputHintCheckerJni.get().setView(new Object());
    }

    @NativeMethods
    interface Natives {
        void setView(Object view);

        boolean isInitializedForTesting(); // IN-TEST

        boolean failedToInitializeForTesting(); // IN-TEST

        boolean hasInputForTesting(); // IN-TEST

        boolean hasInputWithThrottlingForTesting(); // IN-TEST
    }
}
