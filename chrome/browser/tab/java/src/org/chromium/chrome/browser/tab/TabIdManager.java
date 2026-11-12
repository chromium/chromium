// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Maintains a monotonically increasing ID that is used for uniquely identifying {@link Tab}s. This
 * class is responsible for ensuring that Tabs created in the same process, across every TabModel,
 * are allocated a unique ID. Note that only the browser process should be generating Tab IDs on the
 * UI thread to prevent collisions.
 *
 * <p>Calling {@link TabIdManager#incrementIdCounterTo(int)} will ensure new {@link Tab}s get IDs
 * greater than or equal to the parameter passed to that method. This should be used when doing
 * things like loading persisted {@link Tab}s from disk on process start to ensure all new {@link
 * Tab}s don't have id collision.
 */
@NullMarked
public final class TabIdManager {
    private static @Nullable TabIdManager sInstance;

    private @TabId int mIdCounter;

    /** Returns the Singleton instance of the TabIdManager. Must be called on the UI thread. */
    public static TabIdManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new TabIdManager();
        }
        return sInstance;
    }

    /**
     * Validates {@code id} and increments the internal counter to make sure future ids don't
     * collide.
     *
     * @param id The current id. May be {@link Tab#INVALID_TAB_ID}.
     * @return A new id if {@code id} was {@link Tab#INVALID_TAB_ID}, or {@code id}.
     */
    public @TabId int generateValidId(@TabId int id) {
        ThreadUtils.assertOnUiThread();
        if (id == Tab.INVALID_TAB_ID) id = mIdCounter;
        incrementIdCounterTo(id + 1);
        return id;
    }

    /**
     * Ensures the counter is at least as high as the specified value. The counter should always
     * point to an unused ID (which will be handed out next time a request comes in). Exposed so
     * that anything externally loading tabs and ids can enforce that new tabs start at the correct
     * id.
     *
     * @param id The minimum id we should hand out to the next new tab.
     */
    public void incrementIdCounterTo(@TabId int id) {
        ThreadUtils.assertOnUiThread();
        if (id <= mIdCounter) return;

        mIdCounter = id;
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, mIdCounter);
    }

    private TabIdManager() {
        // Read the shared preference. This has to be done on the critical path to ensure that the
        // myriad Activities that serve as entries into Chrome are all synchronized on the correct
        // maximum Tab ID.
        mIdCounter =
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID);
    }

    public static void resetInstanceForTesting() {
        sInstance = null;
    }
}
