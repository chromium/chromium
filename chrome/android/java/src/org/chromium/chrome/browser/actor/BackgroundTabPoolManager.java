// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import android.util.ArrayMap;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;

/**
 * Manages transient, leased {@link BackgroundTabPool} instances per profile token.
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
    private static final ArrayMap<String, PoolHolder> sHolders = new ArrayMap<>();

    private static @Nullable BackgroundTabPool sPoolForTesting;

    private static class PoolHolder implements Destroyable {
        private final String mProfileToken;
        private @Nullable BackgroundTabPool mPool;
        private int mLeaseCount;

        PoolHolder(String profileToken) {
            mProfileToken = profileToken;
        }

        BackgroundTabPool acquire() {
            assertOnUiThread();
            if (mPool == null || mPool.isDestroyed()) {
                mPool = new BackgroundTabPool(mProfileToken, this::checkIdleAndDestroy);
                mLeaseCount = 0;
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
                sHolders.remove(mProfileToken);
                if (sHolders.isEmpty()) {
                    clearLastUsedProfileToken();
                }
            }
        }

        @Nullable BackgroundTabPool getPoolForTesting() {
            if (mPool != null && mPool.isDestroyed()) {
                return null;
            }
            return mPool;
        }

        int getLeaseCountForTesting() {
            return mLeaseCount;
        }

        @Override
        public void destroy() {
            assertOnUiThread();
            if (mPool != null) {
                if (!mPool.isDestroyed()) {
                    mPool.destroy();
                }
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
        String profileToken = new ProfileResolver().tokenize(profile);
        persistLastUsedProfileToken(profileToken);
        return acquire(profileToken);
    }

    private static void persistLastUsedProfileToken(String profileToken) {
        // TODO(crbug.com/491791326): Support tracking and persisting tokens for multiple profiles.
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN, profileToken);
    }

    /**
     * Acquires a leased {@link BackgroundTabPool} instance for the given profile token. Callers
     * must balance each call with {@link #release(BackgroundTabPool)}.
     *
     * @param profileToken The profile token string to acquire a pool for.
     * @return The leased {@link BackgroundTabPool} instance.
     */
    public static BackgroundTabPool acquire(String profileToken) {
        assertOnUiThread();
        if (sPoolForTesting != null) {
            return sPoolForTesting;
        }
        PoolHolder holder = sHolders.get(profileToken);
        if (holder == null) {
            holder = new PoolHolder(profileToken);
            sHolders.put(profileToken, holder);
        }
        return holder.acquire();
    }

    /**
     * Attempts to restore a {@link BackgroundTabPool} if a profile token was previously persisted.
     *
     * @return The restored {@link BackgroundTabPool} instance, or null if no token exists.
     */
    public static @Nullable BackgroundTabPool restorePoolIfTokenExists() {
        assertOnUiThread();
        if (sPoolForTesting != null) {
            return sPoolForTesting;
        }
        // TODO(crbug.com/491791326): Support synchronous restoration across multiple profiles
        // during startup.
        String token =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN, null);
        if (token == null || token.isEmpty()) {
            return null;
        }
        return acquire(token);
    }

    /** Clears the persisted last-used profile token in {@link ChromeSharedPreferences}. */
    public static void clearLastUsedProfileToken() {
        // TODO(crbug.com/491791326): Support multi-profile token management and selective clearing.
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN);
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
        PoolHolder holder = sHolders.get(pool.getProfileToken());
        if (holder != null) {
            holder.release(pool);
        }
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
        String profileToken = new ProfileResolver().tokenize(profile);
        PoolHolder holder = sHolders.get(profileToken);
        return holder != null ? holder.getPoolForTesting() : null;
    }

    /**
     * Returns the current lease count for the given profile for testing.
     *
     * @param profile The {@link Profile} associated with the pool.
     * @return The integer lease count.
     */
    public static int getLeaseCountForTesting(Profile profile) {
        String profileToken = new ProfileResolver().tokenize(profile);
        PoolHolder holder = sHolders.get(profileToken);
        return holder != null ? holder.getLeaseCountForTesting() : 0;
    }

    /** Resets all static state and destroys all pool holders for testing. */
    public static void resetForTesting() {
        assertOnUiThread();
        sPoolForTesting = null;
        for (int i = 0; i < sHolders.size(); i++) {
            sHolders.valueAt(i).destroy();
        }
        sHolders.clear();
        clearLastUsedProfileToken();
    }
}
