// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

import android.os.Build;
import android.util.CloseGuard;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.core.util.Preconditions;

/**
 * Helper for accessing CloseGuard on API levels that support it.
 *
 * <p>All operations are no-ops on non-supported API levels.
 */
@SuppressWarnings("NotCloseable")
final class CloseGuardHelper {
    private final CloseGuardImpl mImpl;

    private CloseGuardHelper(CloseGuardImpl impl) {
        mImpl = impl;
    }

    /**
     * Returns a {@link CloseGuardHelper} which defers to the platform close guard if it is
     * available.
     */
    @NonNull
    public static CloseGuardHelper create() {
        if (Build.VERSION.SDK_INT >= 30) {
            return new CloseGuardHelper(new CloseGuardApi30Impl());
        }

        return new CloseGuardHelper(new CloseGuardNoOpImpl());
    }

    /**
     * Initializes the instance with a warning that the caller should have explicitly called the
     * {@code closeMethodName} method instead of relying on finalization.
     *
     * @param closeMethodName non-null name of explicit termination method. Printed by
     *                        warnIfOpen.
     * @throws NullPointerException if closeMethodName is null.
     */
    public void open(@NonNull String closeMethodName) {
        mImpl.open(closeMethodName);
    }

    /** Marks this CloseGuard instance as closed to avoid warnings on finalization. */
    public void close() {
        mImpl.close();
    }

    /**
     * Logs a warning if the caller did not properly cleanup by calling an explicit close method
     * before finalization.
     */
    public void warnIfOpen() {
        mImpl.warnIfOpen();
    }

    private interface CloseGuardImpl {
        void open(@NonNull String closeMethodName);
        void close();
        void warnIfOpen();
    }

    @RequiresApi(30)
    static final class CloseGuardApi30Impl implements CloseGuardImpl {
        private final CloseGuard mPlatformImpl = new CloseGuard();

        @Override
        public void open(@NonNull String closeMethodName) {
            mPlatformImpl.open(closeMethodName);
        }

        @Override
        public void close() {
            mPlatformImpl.close();
        }

        @Override
        public void warnIfOpen() {
            mPlatformImpl.warnIfOpen();
        }
    }

    static final class CloseGuardNoOpImpl implements CloseGuardImpl {
        @Override
        public void open(@NonNull String closeMethodName) {
            Preconditions.checkNotNull(closeMethodName, "CloseMethodName must not be null.");
        }

        @Override
        public void close() {}

        @Override
        public void warnIfOpen() {}
    }
}
