// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Java bridge to handle conditional prerendering using autocomplete results * as the user types
 * into the Omnibox.
 *
 * OmniboxPrerender takes keystrokes, autocomplete results and navigation actions then feeds
 * them to the (native) AutocompleteActionPredictor. The predictor uses this data to update its
 * database and returns predictions on what page, if any, to pre-render or pre-connect.
 *
 */
public class OmniboxPrerender {
    private long mNativeOmniboxPrerender;

    /**
     * Constructor for creating a OmniboxPrerender instanace.
     */
    public OmniboxPrerender() {
        mNativeOmniboxPrerender = OmniboxPrerenderJni.get().init(OmniboxPrerender.this);
    }

    /**
     * Clears the transitional matches. This should be called when the user stops typing into
     * the omnibox (e.g. when navigating away, closing the keyboard or changing tabs)
     *
     * @param profile profile instance corresponding to the active profile.
     */
    public void clear(Profile profile) {
        OmniboxPrerenderJni.get().clear(mNativeOmniboxPrerender, OmniboxPrerender.this, profile);
    }

    /**
     * Initializes the underlying action predictor for a given profile instance. This should be
     * called as soon as possible as the predictor must register for certain notifications to
     * properly initialize before providing predictions and updated its learning database.
     *
     * @param profile profile instance corresponding to active profile.
     */
    public void initializeForProfile(Profile profile) {
        OmniboxPrerenderJni.get().initializeForProfile(
                mNativeOmniboxPrerender, OmniboxPrerender.this, profile);
    }

    /**
     * Potentailly invokes a pre-render or pre-connect given the url typed into the omnibox and
     * a corresponding autocomplete result. This should be invoked everytime the omnibox changes
     * (e.g. As the user types characters this method should be invoked at least once per character)
     *
     * @param url url in the omnibox.
     * @param currentUrl url the current tab is displaying.
     * @param nativeAutocompleteResult native pointer to an autocomplete result.
     * @param profile profile instance corresponding to the active profile.
     * @param tab The tab whose webcontent's to use.
     */
    public void prerenderMaybe(String url, String currentUrl, long nativeAutocompleteResult,
            Profile profile, Tab tab) {
        OmniboxPrerenderJni.get().prerenderMaybe(mNativeOmniboxPrerender, OmniboxPrerender.this,
                url, currentUrl, nativeAutocompleteResult, profile, tab);
    }

    @NativeMethods
    interface Natives {
        long init(OmniboxPrerender caller);
        void clear(long nativeOmniboxPrerender, OmniboxPrerender caller, Profile profile);
        void initializeForProfile(
                long nativeOmniboxPrerender, OmniboxPrerender caller, Profile profile);
        void prerenderMaybe(long nativeOmniboxPrerender, OmniboxPrerender caller, String url,
                String currentUrl, long nativeAutocompleteResult, Profile profile, Tab tab);
    }
}
