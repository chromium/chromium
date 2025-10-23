// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.RestrictTo;
import androidx.annotation.StringDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Constants for ProcessGlobalConfig shared between chromium and AndroidX. */
@NullMarked
public final class ProcessGlobalConfigConstants {
    private ProcessGlobalConfigConstants() {}

    /** @hide */
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    @StringDef(
            value = {
                DATA_DIRECTORY_SUFFIX,
                DATA_DIRECTORY_BASE_PATH,
                CACHE_DIRECTORY_BASE_PATH,
                CONFIGURE_PARTITIONED_COOKIES,
                UI_THREAD_STARTUP_MODE
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

    /**
     * Key for setting Ui thread startup mode in the process global config map that is read in
     * chromium via reflection into AndroidX class.
     */
    public static final String UI_THREAD_STARTUP_MODE = "UI_THREAD_STARTUP_MODE";

    /**
     * WebView's UI thread initialization may or may not block the UI thread, depending on the
     * internal implementation.
     */
    public static final int UI_THREAD_STARTUP_MODE_DEFAULT = -1;

    /**
     * WebView's UI thread initialization will run in a single, continuous block. This can
     * negatively impact responsiveness and may lead to ANRs.
     */
    public static final int UI_THREAD_STARTUP_MODE_SYNC = 0;

    /**
     * WebView's UI thread initialization will run in multiple blocks to improve responsiveness.
     * However, some of these blocks may still be long enough to cause ANRs
     */
    public static final int UI_THREAD_STARTUP_MODE_ASYNC_LONG_TASKS = 1;

    /**
     * WebView's UI thread initialization will run in multiple short blocks to improve
     * responsiveness, reducing the risk of ANRs compared to {@link
     * #UI_THREAD_STARTUP_MODE_ASYNC_LONG_TASKS}.
     */
    public static final int UI_THREAD_STARTUP_MODE_ASYNC_SHORT_TASKS = 2;

    /**
     * WebView's UI thread initialization will run in multiple very short blocks to improve app
     * responsiveness and make ANRs unlikely compared to {@link
     * #UI_THREAD_STARTUP_MODE_ASYNC_LONG_TASKS} or {@link
     * #UI_THREAD_STARTUP_MODE_ASYNC_SHORT_TASKS}
     */
    public static final int UI_THREAD_STARTUP_MODE_ASYNC_VERY_SHORT_TASKS = 3;

    /**
     * This is the same as {@link #UI_THREAD_STARTUP_MODE_ASYNC_VERY_SHORT_TASKS} in addition to
     * stopping {@link androidx.webkit.WebViewCompat#isMultiProcessEnabled()} check from triggering
     * startup.
     */
    public static final int UI_THREAD_STARTUP_MODE_ASYNC_PLUS_MULTI_PROCESS = 4;
}
