// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;

/**
 * This class stores the state of the ManualFilling-components. It holds {@link ManualFillingState}s
 * associated to {@link WebContents} until explicitly destroyed.
 *
 * Example use:
 * <code>
 *    ManualFillingStateCache cache = new ManualFillingCache();
 *    @NonNull WebContents w1 = [...];
 *    @NonNull WebContents w2 = [...];
 *    assert cache.getStateFor(w1) == cache.getStateFor(w1);
 *    assert cache.getStateFor(w1) != cache.getStateFor(w2);
 *    assert cache.getStateFor(null) != cache.getStateFor(null);
 *    cache.destroyStateFor(w1); // State for w1 cleaned; reference to w2 dropped.
 *    caches.destroy(); // State for w2 cleaned; reference to w2 dropped.
 * </code>
 */
class ManualFillingStateCache {
    private final HashMap<WebContents, ManualFillingState> mStatesForWebContents = new HashMap<>();

    ManualFillingStateCache() {}

    /**
     * @see #getStateFor(WebContents)
     * @param tab A {@link Tab} for whose {@link WebContents} a state is needed.
     * @return A {@link ManualFillingState}. Never null.
     */
    ManualFillingState getStateFor(Tab tab) {
        return getStateFor(tab.getWebContents());
    }

    /**
     * Returns a state for the given WebContents and caches it. If the given WebContents are null,
     * the returned empty state is not cached.
     * @param webContents {@link WebContents} for which a state is needed.
     * @return A {@link ManualFillingState}. Never null.
     */
    ManualFillingState getStateFor(@Nullable WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) {
            // If state is requested for destroyed or invalid WebContents, it returns a null object.
            return new ManualFillingState(null);
        }
        ManualFillingState state = mStatesForWebContents.get(webContents);
        if (state != null) return state;
        state = new ManualFillingState(webContents);
        mStatesForWebContents.put(webContents, state);
        return state;
    }

    /**
     * Destroys all held states and removes the held references to the WebContents they belong to.
     */
    void destroy() {
        for (ManualFillingState userState : mStatesForWebContents.values()) userState.destroy();
        mStatesForWebContents.clear();
    }

    /**
     * @see #destroyStateFor(WebContents)
     * @param tab The tab whose WebContents are going to be destroyed.
     */
    void destroyStateFor(Tab tab) {
        destroyStateFor(tab.getWebContents());
    }

    /**
     * Ensures a reference to WebContents isn't held longer than necessary so GC can collect it.
     * @param webContents The WebContents about to be destroyed and should not be held any longer.
     */
    void destroyStateFor(WebContents webContents) {
        if (webContents != null) { // No need to check isDestroyed since the object is only a key.
            getStateFor(webContents).destroy();
            mStatesForWebContents.remove(webContents);
        }
    }
}
