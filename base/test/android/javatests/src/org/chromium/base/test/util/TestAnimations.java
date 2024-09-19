// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.IBinder;
import android.provider.Settings;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.test.InstrumentationRegistry;

import org.chromium.base.ContextUtils;
import org.chromium.base.JavaUtils;
import org.chromium.base.Log;

import java.io.IOException;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Method;
import java.util.Arrays;

/** Enables / disables window Android animations. */
public class TestAnimations {

    private static Method sSetAnimationScalesMethod;
    private static Method sGetAnimationScalesMethod;
    private static Object sWindowManagerObject;

    /** Enable animations for the test class / method (default is disabled). */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableAnimations {}

    private static final String TAG = "TestAnimations";
    // Start as null because we cannot trust that animations were left enabled by the previous test.
    private static @Nullable Boolean sAnimationsEnabled;

    /** Enable / disable animations based on @EnableAnimations annotation. */
    public static void reset(Class<?> testClass, @Nullable Method testMethod) {
        boolean enable =
                testClass.getAnnotation(EnableAnimations.class) != null
                        || (testMethod != null
                                && testMethod.getAnnotation(EnableAnimations.class) != null);
        setEnabled(enable);
    }

    /** Enable / disable animations. */
    public static void setEnabled(boolean value) {
        if (sAnimationsEnabled == null) {
            float curAnimationScale =
                    Settings.Global.getFloat(
                            ContextUtils.getApplicationContext().getContentResolver(),
                            Settings.Global.ANIMATOR_DURATION_SCALE,
                            1);
            if (curAnimationScale == 1) {
                sAnimationsEnabled = true;
            } else if (curAnimationScale == 0) {
                sAnimationsEnabled = false;
            } else {
                Log.i(TAG, "Animations scale was %s", curAnimationScale);
            }
        }
        if (sAnimationsEnabled != null && value == sAnimationsEnabled) {
            Log.i(TAG, "Animations are already %s.", value ? "enabled" : "disabled");
            return;
        }
        float scaleFactor = value ? 1.0f : 0.0f;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            setViaSupportedApi(scaleFactor);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            setViaShell(scaleFactor);
        } else {
            try {
                setViaReflection(scaleFactor);
            } catch (Exception e) {
                // If this fails, add required manifest permission via:
                // deps += [ "//testing/android/instrumentation:test_runner_permissions_java" ]
                Log.i(TAG, "Could not disable animations.");
                return;
            }
        }
        sAnimationsEnabled = value;
        Log.i(TAG, "Animations are now %s.", value ? "enabled" : "disabled");
    }

    private static void executeShellCommand(String command) {
        try {
            InstrumentationRegistry.getInstrumentation()
                    .getUiAutomation()
                    .executeShellCommand(command)
                    .close();
        } catch (IOException e) {
            JavaUtils.throwUnchecked(e);
        }
    }

    @RequiresApi(VERSION_CODES.S)
    private static void setViaShell(float scaleFactor) {
        // Set animation scales through settings through shell commands in S+.
        String[] commands = {
            "settings put global animator_duration_scale " + scaleFactor,
            "settings put global transition_animation_scale " + scaleFactor,
            "settings put global window_animation_scale " + scaleFactor
        };
        // This must be 3 separate commands because StringTokenizer is used under-the-hood to
        // separate args (cannot do "sh -c 'quoted commands'").
        for (String command : commands) {
            executeShellCommand(command);
        }
    }

    @SuppressWarnings("SoonBlockedPrivateApi")
    private static void setViaReflection(float scaleFactor) {
        try {
            if (sWindowManagerObject == null) {
                Class<?> windowManagerStubClazz = Class.forName("android.view.IWindowManager$Stub");
                Method asInterface =
                        windowManagerStubClazz.getDeclaredMethod("asInterface", IBinder.class);
                Class<?> serviceManagerClazz = Class.forName("android.os.ServiceManager");
                Method getService =
                        serviceManagerClazz.getDeclaredMethod("getService", String.class);
                Class<?> windowManagerClazz = Class.forName("android.view.IWindowManager");
                sSetAnimationScalesMethod =
                        windowManagerClazz.getDeclaredMethod("setAnimationScales", float[].class);
                sGetAnimationScalesMethod =
                        windowManagerClazz.getDeclaredMethod("getAnimationScales");
                IBinder windowManagerBinder = (IBinder) getService.invoke(null, "window");
                sWindowManagerObject = asInterface.invoke(null, windowManagerBinder);
            }

            float[] scaleFactors = (float[]) sGetAnimationScalesMethod.invoke(sWindowManagerObject);
            Arrays.fill(scaleFactors, scaleFactor);
            sSetAnimationScalesMethod.invoke(sWindowManagerObject, scaleFactors);
        } catch (Exception e) {
            JavaUtils.throwUnchecked(e);
        }
    }

    @RequiresApi(VERSION_CODES.TIRAMISU)
    private static void setViaSupportedApi(float scaleFactor) {
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .setAnimationScale(scaleFactor);
    }
}
