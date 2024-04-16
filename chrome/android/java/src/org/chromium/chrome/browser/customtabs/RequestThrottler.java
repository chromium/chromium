// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.SparseArray;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Applications are throttled in two ways:
 * (a) Cannot issue mayLaunchUrl() too often.
 * (b) Will be banned from prerendering if too many failed attempts are registered.
 *
 * The first throttling is handled by {@link updateStatsAndReturnIfAllowed}, and the second one
 * is persisted to disk and handled by {@link isPrerenderingAllowed()}.
 *
 * This class is *not* thread-safe.
 */
class RequestThrottler {
    // These are for (a).
    private static final long MIN_DELAY = 100;
    private static final long MAX_DELAY = 10000;
    private long mLastRequestMs = -1;
    private long mDelayMs = MIN_DELAY;

    // These are for (b)
    private static final float MAX_SCORE = 10;
    // TODO(lizeb): Control this value using Finch.
    private static final long BAN_DURATION_MS = DateUtils.DAY_IN_MILLIS * 7;
    private static final long FORGET_AFTER_MS = DateUtils.DAY_IN_MILLIS * 14;
    private static final float ALPHA = MAX_SCORE / BAN_DURATION_MS;
    private static final String PREFERENCES_NAME = "customtabs_client_bans";
    private static final String SCORE = "score_";
    private static final String LAST_REQUEST = "last_request_";
    private static final String BANNED_UNTIL = "banned_until_";

    private static final AtomicBoolean sAccessedSharedPreferences = new AtomicBoolean();
    private static SparseArray<RequestThrottler> sUidToThrottler;

    private final SharedPreferences mSharedPreferences;
    private final int mUid;
    private float mScore;
    private long mLastPrerenderRequestMs;
    private long mBannedUntilMs;
    private String mUrl;

    /**
     * Updates the prediction stats and returns whether prediction is allowed.
     *
     * The policy is:
     * 1. If the client does not wait more than mDelayMs, decline the request.
     * 2. If the client waits for more than mDelayMs but less than 2*mDelayMs, accept the request
     *    and double mDelayMs.
     * 3. If the client waits for more than 2*mDelayMs, accept the request and reset mDelayMs.
     *
     * And: 100ms <= mDelayMs <= 10s.
     *
     * This way, if an application sends a burst of requests, it is quickly seriously throttled. If
     * it stops being this way, back to normal.
     */
    public boolean updateStatsAndReturnWhetherAllowed() {
        long now = SystemClock.elapsedRealtime();
        long deltaMs = now - mLastRequestMs;
        if (deltaMs < mDelayMs) return false;
        mLastRequestMs = now;
        if (deltaMs < 2 * mDelayMs) {
            mDelayMs = Math.min(MAX_DELAY, mDelayMs * 2);
        } else {
            mDelayMs = MIN_DELAY;
        }
        return true;
    }

    /** @return true if the client is not banned from prerendering. */
    public boolean isPrerenderingAllowed() {
        return System.currentTimeMillis() >= mBannedUntilMs;
    }

    /** Records that a prerender request was made for a given URL. */
    public void registerPrerenderRequest(String url) {
        mUrl = url;
        long now = System.currentTimeMillis();
        mScore = Math.min(MAX_SCORE, mScore - 1 + ALPHA * (now - mLastPrerenderRequestMs));
        mLastPrerenderRequestMs = now;
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putLong(LAST_REQUEST + mUid, mLastPrerenderRequestMs);
        updateBan(editor);
        editor.apply();
    }

    /** Signals that an incoming intent matched with a mayLaunchUrl() call.
     *
     * This doesn't necessarily match a prerender request, as not all mayLaunchUrl() calls result in
     * prerender requests.
     *
     * @param url URL the matched intent refers to.
     */
    public void registerSuccess(String url) {
        // (a) Back to the minimum delay.
        mDelayMs = MIN_DELAY;
        mLastRequestMs = -1;
        // (b) Note a success.
        // Give +1 even is this doesn't match an actual prerender.
        int bonus = 1;
        if (TextUtils.equals(mUrl, url)) {
            bonus = 2;
            mUrl = null;
        }
        mScore = Math.min(MAX_SCORE, mScore + bonus);
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        updateBan(editor);
        editor.apply();
    }

    /** @return the {@link Throttler} for a given UID. */
    public static RequestThrottler getForUid(int uid) {
        if (sUidToThrottler == null) {
            sUidToThrottler = new SparseArray<>();
            purgeOldEntries();
        }
        RequestThrottler throttler = sUidToThrottler.get(uid);
        if (throttler == null) {
            throttler = new RequestThrottler(uid);
            sUidToThrottler.put(uid, throttler);
        }
        return throttler;
    }

    /** Banning policy:
     * - If the current score is >= 0, allow the request, otherwise the UID is
     *   banned for {@link BAN_DURATION_MS}.
     * - The initial score is {@link MAX_SCORE}
     * - For each request:
     *     score = Min(MAX_SCORE, score - 1 + ALPHA * timeSinceLastRequest)
     *   ALPHA = MAX_SCORE / BAN_DURATION_MS, such that a banned UID would replenish its score at
     *   the same speed as an inactive one.
     * - For each *success*:
     *     score = Min(MAX_SCORE, score + 2)
     *   with +1 for an Intent matching a mayLaunchUrl() call, and +1 if it matches a prerender
     *   request.
     * So, in "steady state", a 50% hit rate is tolerated.
     */
    private void updateBan(SharedPreferences.Editor editor) {
        if (mScore <= 0) {
            mScore = MAX_SCORE;
            mBannedUntilMs = System.currentTimeMillis() + BAN_DURATION_MS;
            editor.putLong(BANNED_UNTIL + mUid, mBannedUntilMs);
        }
        editor.putFloat(SCORE + mUid, mScore);
    }

    private RequestThrottler(int uid) {
        mSharedPreferences =
                ContextUtils.getApplicationContext().getSharedPreferences(PREFERENCES_NAME, 0);
        mUid = uid;
        mScore = mSharedPreferences.getFloat(SCORE + uid, MAX_SCORE);
        mLastPrerenderRequestMs = mSharedPreferences.getLong(LAST_REQUEST + uid, 0);
        mBannedUntilMs = mSharedPreferences.getLong(BANNED_UNTIL + uid, 0);
    }

    /** Resets the banning state. */
    void reset() {
        if (sUidToThrottler != null) sUidToThrottler.remove(mUid);
        mSharedPreferences
                .edit()
                .remove(SCORE + mUid)
                .remove(LAST_REQUEST + mUid)
                .remove(BANNED_UNTIL + mUid)
                .apply();
    }

    /** Bans from prerendering. Used for testing. */
    void ban() {
        mScore = -1;
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        updateBan(editor);
        editor.apply();
    }

    /**
     * Loads the SharedPreferences in the background.
     *
     * <p>SharedPreferences#edit() blocks until the preferences are loaded from disk. This results
     * in a StrictMode violation as this may be called from the UI thread. This loads the
     * preferences in the background to hopefully avoid the violation (if the next edit() call
     * happens once the preferences are loaded).
     */
    // TODO(crbug.com/40479664): Fix this properly.
    @SuppressLint("CommitPrefEdits")
    static void loadInBackground() {
        boolean alreadyDone = !sAccessedSharedPreferences.compareAndSet(false, true);
        if (alreadyDone) return;
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(PREFERENCES_NAME, 0)
                            .edit();
                });
    }

    /** Removes all the UIDs that haven't been seen since at least {@link FORGET_AFTER_MS}. */
    private static void purgeOldEntries() {
        SharedPreferences sharedPreferences =
                ContextUtils.getApplicationContext().getSharedPreferences(PREFERENCES_NAME, 0);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        long now = System.currentTimeMillis();
        for (Map.Entry<String, ?> entry : sharedPreferences.getAll().entrySet()) {
            if (entry == null) continue;
            String key = entry.getKey();
            if (key == null || !key.startsWith(LAST_REQUEST)) continue;
            long lastRequestMs;
            try {
                lastRequestMs = (Long) entry.getValue();
            } catch (NumberFormatException e) {
                continue;
            }
            if (now - lastRequestMs >= FORGET_AFTER_MS) {
                String uid = key.substring(LAST_REQUEST.length());
                editor.remove(SCORE + uid).remove(LAST_REQUEST + uid).remove(BANNED_UNTIL + uid);
            }
        }
        editor.apply();
    }

    static void purgeAllEntriesForTesting() {
        SharedPreferences sharedPreferences =
                ContextUtils.getApplicationContext().getSharedPreferences(PREFERENCES_NAME, 0);
        sharedPreferences.edit().clear().apply();
        if (sUidToThrottler != null) sUidToThrottler.clear();
    }
}
