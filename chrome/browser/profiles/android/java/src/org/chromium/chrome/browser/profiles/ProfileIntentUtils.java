// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utilities for adding / retrieving {@link Profile} information from intents. */
@NullMarked
public class ProfileIntentUtils {
    /* package */ static final String PROFILE_INTENT_KEY;

    static {
        PROFILE_INTENT_KEY = ContextUtils.getApplicationContext().getPackageName() + "_profile_key";
    }

    /** Return whether the intent has a profile token. */
    public static boolean hasProfileToken(Intent intent) {
        return IntentUtils.isTrustedIntentFromSelf(intent) && intent.hasExtra(PROFILE_INTENT_KEY);
    }

    /**
     * Adds the {@link Profile} information to the intent so that it can be retrieved by the
     * receiving component.
     *
     * @param profile The {@link Profile} that should be associated with the intent.
     * @param intent The intent that will store the {@link Profile} information.
     */
    public static void addProfileToIntent(Profile profile, Intent intent) {
        String profileToken = new ProfileResolver().tokenize(profile);
        intent.putExtra(PROFILE_INTENT_KEY, profileToken);
        IntentUtils.addTrustedIntentExtras(intent);
    }

    /**
     * Retrieves the {@link Profile} associated with the given intent if one exists. This requires
     * the intent be marked as trusted for it to attempt to retrieve the {@link Profile}.
     *
     * @param intent The intent containing the {@link Profile} key.
     * @param profileCallback The callback to be notified if the {@link Profile} is resolved, or
     *     null if the {@link Profile} is not resolved. If the {@link Profile} is already loaded,
     *     this callback will be notified synchronously.
     */
    public static void retrieveProfileFromIntent(
            Intent intent, Callback<@Nullable Profile> profileCallback) {
        String profileToken = intent.getStringExtra(PROFILE_INTENT_KEY);
        if (!IntentUtils.isTrustedIntentFromSelf(intent)) {
            assert profileToken == null : "Non-null profile token passed on non-trusted intent.";
            profileCallback.onResult(null);
            return;
        }
        if (profileToken == null) {
            profileCallback.onResult(null);
            return;
        }
        new ProfileResolver().resolveProfile(profileToken, profileCallback);
    }
}
