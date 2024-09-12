// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.RestrictTo;
import androidx.annotation.StringDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Constants for ProcessGlobalConfig shared between chromium and AndroidX. */
public final class ProcessGlobalConfigConstants {
    private ProcessGlobalConfigConstants() {}

    /** @hide */
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @StringDef(
            value = {
                DATA_DIRECTORY_SUFFIX,
                DATA_DIRECTORY_BASE_PATH,
                CACHE_DIRECTORY_BASE_PATH,
                CONFIGURE_PARTITIONED_COOKIES
            })
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.PARAMETER, ElementType.METHOD})
    public @interface ProcessGlobalConfigMapKey {}

    /**
     * Key for the data directory suffix in the process global config map that is read in chromium
     * via reflection into AndroidX class.
     */
    public static final String DATA_DIRECTORY_SUFFIX = "DATA_DIRECTORY_SUFFIX";

    /**
     * Key for the data directory base path in the process global config map that is read in
     * chromium via reflection into AndroidX class.
     */
    public static final String DATA_DIRECTORY_BASE_PATH = "DATA_DIRECTORY_BASE_PATH";

    /**
     * Key for the cache directory base path in the process global config map that is read in
     * chromium via reflection into AndroidX class.
     */
    public static final String CACHE_DIRECTORY_BASE_PATH = "CACHE_DIRECTORY_BASE_PATH";

    /**
     * Key for configuring if partitioned cookies are enabled in the process global config map that
     * is read in chromium via reflection into AndroidX class.
     */
    public static final String CONFIGURE_PARTITIONED_COOKIES = "CONFIGURE_PARTITIONED_COOKIES";
}
