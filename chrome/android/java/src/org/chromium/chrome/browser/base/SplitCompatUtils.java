// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.base.compat.ApiHelperForO;

/** Utils for compatibility with isolated splits. */
public class SplitCompatUtils {
    private SplitCompatUtils() {}

    /**
     * Gets the obfuscated name for the passed in class name. Important: this MUST be called with a
     * string literal, otherwise @IdentifierNameString will not work.
     */
    @IdentifierNameString
    public static String getIdentifierName(String className) {
        return className;
    }

    /** Creates a context which can be used to load code and resources in the chrome split. */
    public static Context createChromeContext(Context base) {
        // Isolated splits are only supported in O+, so just return the base context on other
        // versions, since this will have access to all splits.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return base;
        }
        try {
            return ApiHelperForO.createContextForSplit(base, "chrome");
        } catch (PackageManager.NameNotFoundException e) {
            // This application class should not be used if the chrome split does not exist.
            throw new RuntimeException(e);
        }
    }

    /**
     * Constructs a new instance of the given class name using the class loader from the context.
     */
    public static Object newInstance(Context context, String className) {
        try {
            return context.getClassLoader().loadClass(className).newInstance();
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }
}
