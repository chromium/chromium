// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A simple single-argument callback to handle the result of a computation that must be called
 * exactly once.
 *
 * @param <T> The type of the computation's result.
 */
@NullMarked
public class RequiredCallback<T extends @Nullable Object> implements Callback<T> {
    // Enforces (under test) that this callback is invoked before it is GC'd.
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private @Nullable Callback<T> mCallback;

    public RequiredCallback(Callback<T> callback) {
        mCallback = callback;
    }

    @Override
    public void onResult(T result) {
        assert mCallback != null : "Callback was already called.";
        mCallback.onResult(result);
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        mCallback = null;
    }
}
