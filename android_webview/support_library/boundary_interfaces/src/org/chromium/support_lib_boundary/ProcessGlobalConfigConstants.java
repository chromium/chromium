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
     * The default mode for the UI thread startup.
     *
     * <p>The UI thread may or may not be blocked during browser startup, depending on WebView's
     * internal implementation.
     */
    public static final int DEFAULT_UI_THREAD_STARTUP = -1;

    /** The UI thread is blocked completely during browser startup. */
    public static final int SYNC_UI_THREAD_STARTUP = 0;

    /**
     * The UI thread is not blocked completely during browser startup. However, we still have
     * long-running tasks that might trigger ANRs.
     */
    public static final int LONG_TASKS_ASYNC_UI_THREAD_STARTUP = 1;

    /**
     * The UI thread is not blocked completely during browser startup. However, we still have
     * long-running tasks that might trigger ANRs. The difference between this and {@link
     * ASYNC_UI_STARTUP_MODE_ONE} is that the long-running tasks are shorter.
     */
    public static final int SHORTER_TASKS_ASYNC_UI_THREAD_STARTUP = 2;

    /**
     * The UI thread is not blocked completely during browser startup. There are no long-running
     * tasks that might trigger ANRs.
     */
    public static final int SHORT_TASKS_ASYNC_UI_THREAD_STARTUP = 3;
}
