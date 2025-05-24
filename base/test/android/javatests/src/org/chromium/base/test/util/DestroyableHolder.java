// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.Nullable;

/**
 * Similar to an optional, either holds an object or not. But when the reference changes, it will
 * call destroy on the previous value. This is helpful for tests that sometimes test destroy inside
 * of test cases, and then otherwise destroy in tear down.
 *
 * @param <E> The type of the object held.
 */
public class DestroyableHolder<E extends Destroyable> {
    private @Nullable E mTarget;

    public void set(E target) {
        if (mTarget != null) {
            mTarget.destroy();
        }
        mTarget = target;
    }

    public @Nullable E get() {
        return mTarget;
    }

    public void destroy() {
        set(null);
    }
}
