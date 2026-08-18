// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.lifetime;

import org.chromium.build.annotations.NullMarked;

/**
 * Utility class that help ensure destruction of objects happens only once.
 *
 * <p>This class does not guarantee thread safety. When thread safety is desired, please use {@link
 * org.chromium.base.ThreadUtils.ThreadChecker}.
 *
 * <p>To use:
 *
 * <ol>
 *   <li>In constructor of an instance a DestroyChecker field should be initialized with a new
 *       DestroyChecker.
 *   <li>All of the methods that need to ensure that the object is used safely, should call {@link
 *       #checkNotDestroyed()} to make sure that DestroyChecker hasn't been destroyed.
 *   <li>When the guarded object is destroyed, it should be enough to call {@link #destroy()} on the
 *       DestroyChecker. That operation is not idempotent, and it asserts the state of the checker.
 *       It is therefore not necessary to call {@link #checkNotDestroyed()} in that case. It is also
 *       not allowed to call {@link #destroy()} more than once.
 * </ol>
 */
@NullMarked
public class DestroyChecker implements Destroyable {
    private boolean mIsDestroyed;

    @Override
    public void destroy() {
        checkNotDestroyed();
        mIsDestroyed = true;
    }

    /** Returns whether the checker is already destroyed. */
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    /** Checks whether the object is already destroyed and asserts if it is. */
    public void checkNotDestroyed() {
        assert !mIsDestroyed : "Object is already destroyed.";
    }
}
