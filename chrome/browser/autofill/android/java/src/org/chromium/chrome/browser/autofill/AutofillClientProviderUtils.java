// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.ComponentName;
import android.os.Build;
import android.view.autofill.AutofillManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/**
 * Helper functions for using Android Autofill in Chrome.
 */
@JNINamespace("autofill")
public class AutofillClientProviderUtils {
    private static final String AWG_COMPONENT_NAME =
            "com.google.android.gms/com.google.android.gms.autofill.service.AutofillService";

    /**
     * Returns whether all conditions are met for using the Android Autofill framework in Chrome
     * on Android: The AutofillManager exists, is enabled, and its provider is not Autofill with
     * Google.
     **/
    @CalledByNative
    public static boolean isAllowedToUseAndroidAutofillFramework() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return false;
        }
        AutofillManager manager =
                ContextUtils.getApplicationContext().getSystemService(AutofillManager.class);
        if (manager == null || !manager.isEnabled()) {
            return false;
        }
        ComponentName componentName = null;
        try {
            componentName = manager.getAutofillServiceComponentName();
        } catch (Exception e) {
        }
        return componentName != null && !AWG_COMPONENT_NAME.equals(componentName.flattenToString());
    }

    private AutofillClientProviderUtils() {}
}
