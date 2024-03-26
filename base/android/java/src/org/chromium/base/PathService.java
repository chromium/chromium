// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/** This class provides java side access to the native PathService. */
@JNINamespace("base::android")
public abstract class PathService {

    // Must match the value of DIR_MODULE in base/base_paths.h!
    public static final int DIR_MODULE = 3;

    // Prevent instantiation.
    private PathService() {}

    public static void override(int what, String path) {
        PathServiceJni.get().override(what, path);
    }

    @NativeMethods
    interface Natives {
        void override(int what, @JniType("std::string") String path);
    }
}
