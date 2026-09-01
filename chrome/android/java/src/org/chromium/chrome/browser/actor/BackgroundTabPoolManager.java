// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/**
 * Manages transient, leased {@link BackgroundTabPool} instances per {@link Profile}.
 *
 * <p>Enforces dual-invariant teardown: a pool is only destroyed when both:
 *
 * <ol>
 *   <li>The external client lease count is zero (no clients actively using it), AND
 *   <li>The pool contains zero in-memory background tabs (pool is empty).
 * </ol>
 */
@NullMarked
public final class BackgroundTabPoolManager {
    private static final ProfileKeyedMap<PoolHolder> sHolders =
            ProfileKeyedMap.createMapOfDestroyables(ProfileKeyedMap.ProfileSelection.OWN_INSTANCE);

    private static @Nullable BackgroundTabPool sPoolForTesting;

    private static class PoolHolder implements Destroyable {
        private final Profile mProfile;
        private @Nullable BackgroundTabPool mPool;
        private int mLeaseCount;

        PoolHolder(Profile profile) {
            mProfile = profile;
        }

        BackgroundTabPool acquire() {
            assertOnUiThread();
            if (mPool == null) {
                mPool = new BackgroundTabPool(mProfile, this::checkIdleAndDestroy);
            }
            mLeaseCount++;
            return mPool;
        }

        void release(BackgroundTabPool pool) {
            assertOnUiThread();
            assert mPool == pool : "Released pool does not match the active pool.";
            assert mLeaseCount > 0 : "Unbalanced release() call on BackgroundTabPoolManager.";
            if (mLeaseCount > 0) {
                mLeaseCount--;
            }
            checkIdleAndDestroy();
        }

        void checkIdleAndDestroy() {
            assertOnUiThread();
            if (mPool != null && mLeaseCount == 0 && mPool.isEmpty()) {
                mPool.destroy();
                mPool = null;
            }
        }

        @Nullable BackgroundTabPool getPoolForTesting() {
            return mPool;
        }

        int getLeaseCountForTesting() {
            return mLeaseCount;
        }

        @Override
        public void destroy() {
            assertOnUiThread();
            if (mPool != null) {
                mPool.destroy();
                mPool = null;
            }
            mLeaseCount = 0;
        }
    }

    private BackgroundTabPoolManager() {}

    /**
     * Acquires a leased {@link BackgroundTabPool} instance for the given profile. Callers must
     * balance each call with {@link #release(BackgroundTabPool)}.
     *
     * @param profile The {@link Profile} to acquire a pool for.
     * @return The leased {@link BackgroundTabPool} instance.
     */
    public static BackgroundTabPool acquire(Profile profile) {
        assertOnUiThread();
        if (sPoolForTesting != null) {
            return sPoolForTesting;
        }
        assert !profile.isOffTheRecord() : "BackgroundTabPool does not support OTR profiles.";
        return sHolders.getForProfile(profile, PoolHolder::new).acquire();
    }

    /**
     * Releases a lease on the specified {@link BackgroundTabPool}. Triggers teardown if and only if
     * zero leases remain and the pool is empty.
     *
     * @param pool The {@link BackgroundTabPool} to release.
     */
    public static void release(BackgroundTabPool pool) {
        assertOnUiThread();
        if (sPoolForTesting != null) {
            return;
        }
        PoolHolder holder = sHolders.getForProfile(pool.getProfile(), PoolHolder::new);
        holder.release(pool);
    }

    /**
     * Sets a pool instance for testing.
     *
     * @param pool The {@link BackgroundTabPool} to use for testing, or null to clear.
     */
    public static void setPoolForTesting(@Nullable BackgroundTabPool pool) {
        sPoolForTesting = pool;
    }

    /** Returns whether a mock pool for testing is configured. */
    public static boolean hasPoolForTesting() {
        return sPoolForTesting != null;
    }

    /**
     * Returns the active pool instance for the given profile for testing.
     *
     * @param profile The {@link Profile} associated with the pool.
     * @return The active {@link BackgroundTabPool}, or null if none.
     */
    public static @Nullable BackgroundTabPool getPoolForTesting(Profile profile) {
        PoolHolder holder = sHolders.getForProfile(profile, PoolHolder::new);
        return holder.getPoolForTesting();
    }

    /**
     * Returns the current lease count for the given profile for testing.
     *
     * @param profile The {@link Profile} associated with the pool.
     * @return The integer lease count.
     */
    public static int getLeaseCountForTesting(Profile profile) {
        PoolHolder holder = sHolders.getForProfile(profile, PoolHolder::new);
        return holder.getLeaseCountForTesting();
    }

    /** Resets all static state and destroys all pool holders for testing. */
    public static void resetForTesting() {
        assertOnUiThread();
        sPoolForTesting = null;
        sHolders.destroy();
    }
}
