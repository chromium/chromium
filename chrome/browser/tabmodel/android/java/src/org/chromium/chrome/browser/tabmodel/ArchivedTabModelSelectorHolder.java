// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;

import java.util.function.Function;

/**
 * Holds a reference to the ProfileKeyedMap which owns all instance of TabModelSelector, which is
 * pushed to by ArchivedTabModelOrchestrator. This is done as a temporary solutoin to fix a crash
 * when the browsing data is cleared from settings. It should be removed for a more forward-looking
 * crashfix.
 */
public class ArchivedTabModelSelectorHolder {
    private static Function<Profile, TabModelSelector> sArchivedTabModelSelectorFn;

    private ArchivedTabModelSelectorHolder() {}

    /** Sets the instance function used to get access to the archved TabModelSelector. */
    public static void setInstanceFn(
            Function<Profile, TabModelSelector> archivedTabModelSelectorFn) {
        sArchivedTabModelSelectorFn = archivedTabModelSelectorFn;
    }

    /** Given a profile, returns the matching TabModelSelector. */
    public static @Nullable TabModelSelector getInstance(Profile profile) {
        if (profile == null || sArchivedTabModelSelectorFn == null) return null;
        return sArchivedTabModelSelectorFn.apply(profile);
    }
}
