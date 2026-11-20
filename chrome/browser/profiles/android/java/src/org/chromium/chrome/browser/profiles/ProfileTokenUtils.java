// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Base64;

/**
 * Utilities for carrying a {@link Profile} reference through intents, PendingIntents and
 * notifications, so that the receiving side acts on the right Profile.
 *
 * <p>A Profile is represented by the bytes of its {@link ProfileResolver} token, encoded as
 * URL-safe base64. It is stable across restarts and differs between Profiles, including between a
 * regular Profile and its off-the-record Profiles. It is encoded but not opaque: it decodes to the
 * Profile's directory name (and, for off-the-record Profiles, its off-the-record id). Encoded
 * tokens end up persisted (e.g. in PendingIntents and notification tags), so see the comment on the
 * native ProfileToken proto before changing what is encoded.
 */
@NullMarked
public final class ProfileTokenUtils {
    /* package */ static final String PROFILE_TOKEN_EXTRA;

    // Extra holding the raw (not encoded) token, used before tokens were encoded.
    // TODO(crbug.com/585922618): Remove in M163 once intents posted before then have expired.
    /* package */ static final String LEGACY_PROFILE_TOKEN_EXTRA;

    // Separates the feature's tag prefix from the encoded token in notification tags. Tags end up
    // persisted with posted notifications, so this must not change.
    private static final String NOTIFICATION_TAG_SEPARATOR = ":";

    static {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        PROFILE_TOKEN_EXTRA = packageName + "_profile_token";
        LEGACY_PROFILE_TOKEN_EXTRA = packageName + "_profile_key";
    }

    private ProfileTokenUtils() {}

    /** Returns whether the intent is trusted and carries a {@link Profile}. */
    public static boolean hasProfileToken(Intent intent) {
        return IntentUtils.isTrustedIntentFromSelf(intent)
                && (intent.hasExtra(PROFILE_TOKEN_EXTRA)
                        || intent.hasExtra(LEGACY_PROFILE_TOKEN_EXTRA));
    }

    /**
     * Adds the {@link Profile} to the intent so that it can be retrieved by the receiving component
     * with {@link #retrieveProfileFromIntent}. Also marks the intent as trusted and sets its {@link
     * Intent#setIdentifier(String) identifier} to the encoded Profile token so that PendingIntents
     * created from otherwise equal intents for different Profiles are distinct, rather than sharing
     * (and overwriting) one PendingIntent. Per-notification or per-action uniqueness within the
     * same Profile belongs in the PendingIntent {@code requestCode}. Setting the identifier doesn't
     * affect how the intent is resolved.
     *
     * <p>The intent must not already have an identifier set.
     *
     * @param profile The {@link Profile} that should be associated with the intent.
     * @param intent The intent that will store the {@link Profile}.
     */
    public static void addProfileToIntent(Profile profile, Intent intent) {
        assert intent.getIdentifier() == null
                : "Intent already has an identifier: " + intent.getIdentifier();
        String encodedToken = encodeToken(profile);
        intent.putExtra(PROFILE_TOKEN_EXTRA, encodedToken);
        intent.setIdentifier(encodedToken);
        IntentUtils.addTrustedIntentExtras(intent);
    }

    /**
     * Retrieves the {@link Profile} associated with the given intent if one exists. This requires
     * the intent be marked as trusted for it to attempt to retrieve the {@link Profile}.
     *
     * @param intent The intent containing the {@link Profile}.
     * @param profileCallback The callback to be notified if the {@link Profile} is resolved, or
     *     null if the {@link Profile} is not resolved. If the {@link Profile} is already loaded,
     *     this callback will be notified synchronously.
     */
    public static void retrieveProfileFromIntent(
            Intent intent, Callback<@Nullable Profile> profileCallback) {
        if (!IntentUtils.isTrustedIntentFromSelf(intent)) {
            assert !intent.hasExtra(PROFILE_TOKEN_EXTRA)
                            && !intent.hasExtra(LEGACY_PROFILE_TOKEN_EXTRA)
                    : "Profile token passed on non-trusted intent.";
            profileCallback.onResult(null);
            return;
        }
        String encodedToken = IntentUtils.safeGetStringExtra(intent, PROFILE_TOKEN_EXTRA);
        if (encodedToken != null) {
            byte[] token = decodeToken(encodedToken);
            if (token == null) {
                profileCallback.onResult(null);
                return;
            }
            new ProfileResolver().resolveProfileFromBytes(token, profileCallback);
            return;
        }
        String legacyToken = IntentUtils.safeGetStringExtra(intent, LEGACY_PROFILE_TOKEN_EXTRA);
        if (legacyToken == null) {
            profileCallback.onResult(null);
            return;
        }
        new ProfileResolver().resolveProfile(legacyToken, profileCallback);
    }

    /**
     * Returns a notification tag specific to the {@link Profile}, so that each Profile's
     * notifications are separate.
     *
     * @param tagPrefix The feature's notification tag, shared by all Profiles.
     * @param profile The {@link Profile} the notification is for.
     */
    public static String getNotificationTag(String tagPrefix, Profile profile) {
        return buildNotificationTag(tagPrefix, encodeToken(profile));
    }

    /**
     * Returns the notification tag that {@link #getNotificationTag} returns for the {@link Profile}
     * added to the intent with {@link #addProfileToIntent}. Doesn't require native to be
     * initialized, e.g. to dismiss the notification from a receiver.
     *
     * @param tagPrefix The feature's notification tag, shared by all Profiles.
     * @param intent The trusted intent carrying the {@link Profile}.
     * @return The tag, or null if the intent isn't trusted or carries no (current) Profile token.
     */
    public static @Nullable String getNotificationTagFromIntent(String tagPrefix, Intent intent) {
        if (!IntentUtils.isTrustedIntentFromSelf(intent)) return null;
        String encodedToken = IntentUtils.safeGetStringExtra(intent, PROFILE_TOKEN_EXTRA);
        if (encodedToken == null) return null;
        return buildNotificationTag(tagPrefix, encodedToken);
    }

    private static String buildNotificationTag(String tagPrefix, String encodedToken) {
        return tagPrefix + NOTIFICATION_TAG_SEPARATOR + encodedToken;
    }

    private static String encodeToken(Profile profile) {
        byte[] token = new ProfileResolver().tokenizeToBytes(profile);
        return Base64.getUrlEncoder().withoutPadding().encodeToString(token);
    }

    private static byte @Nullable [] decodeToken(String encodedToken) {
        try {
            return Base64.getUrlDecoder().decode(encodedToken);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }
}
