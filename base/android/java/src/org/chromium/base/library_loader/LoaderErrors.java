// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * These are the possible failures from the LibraryLoader
 */
@IntDef({LoaderErrors.NORMAL_COMPLETION, LoaderErrors.FAILED_TO_REGISTER_JNI,
        LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED, LoaderErrors.NATIVE_LIBRARY_WRONG_VERSION,
        LoaderErrors.NATIVE_STARTUP_FAILED})
@Retention(RetentionPolicy.SOURCE)
public @interface LoaderErrors {
    int NORMAL_COMPLETION = 0;
    int FAILED_TO_REGISTER_JNI = 1;
    int NATIVE_LIBRARY_LOAD_FAILED = 2;
    int NATIVE_LIBRARY_WRONG_VERSION = 3;
    int NATIVE_STARTUP_FAILED = 4;
}
