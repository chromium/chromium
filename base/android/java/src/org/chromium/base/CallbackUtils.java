// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/** Utilities for interacting with {@link Callback}s. */
public class CallbackUtils {
    /**
     * @see #emptyCallback() to avoid unchecked generic checks.
     */
    private static final Callback DO_NOTHING_CALLBACK = (unused) -> {};

    /** Use for runnables where you need no action to be taken. */
    private static final Runnable DO_NOTHING_RUNNABLE = () -> {};

    /** Returns a Singleton {@link Callback} to be used where you need no action to be taken. */
    @SuppressWarnings("unchecked")
    public static <T> Callback<T> emptyCallback() {
        return DO_NOTHING_CALLBACK;
    }

    /** Returns a Singleton {@link Runnable} to be used where you need no action to be taken. */
    public static Runnable emptyRunnable() {
        return DO_NOTHING_RUNNABLE;
    }
}
