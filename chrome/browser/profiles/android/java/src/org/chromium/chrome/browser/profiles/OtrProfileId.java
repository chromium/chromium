// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Wrapper that allows passing a OtrProfileId reference around in the Java layer.
 *
 * <p>An {@link OtrProfileId} identifies an off-the-record profile relative to its owning regular
 * {@link Profile}; it does not uniquely identify a profile across different regular profiles.
 */
@NullMarked
public class OtrProfileId {
    private final String mProfileId;
    // OtrProfileId value should be same with Profile::OtrProfileId::PrimaryID in native.
    private static final OtrProfileId sPrimaryOtrProfileId =
            new OtrProfileId("profile::primary_otr");

    @CalledByNative
    public OtrProfileId(@JniType("std::string") String profileId) {
        assert profileId != null;
        mProfileId = profileId;
    }

    @CalledByNative
    private @JniType("std::string") String getProfileId() {
        return mProfileId;
    }

    /** Creates a unique profile id by appending a unique serial number to the given prefix. */
    public static OtrProfileId createUnique(String profileIdPrefix) {
        return OtrProfileIdJni.get().createUniqueOtrProfileId(profileIdPrefix);
    }

    /** Creates a new unique Incognito CCT profile id. */
    public static OtrProfileId createUniqueIncognitoCctId() {
        return OtrProfileIdJni.get().createUniqueIncognitoCctId();
    }

    /**
     * Deconstruct OtrProfileId to a string representation. Deconstructed version will be used to
     * pass the id to the native side and /components/ layer.
     *
     * @param otrProfileId An OtrProfileId instance.
     * @return A string that represents the given otrProfileId.
     */
    @CalledByNative
    public static @JniType("std::string") @Nullable String serialize(
            @Nullable OtrProfileId otrProfileId) {
        // The OtrProfileId might be null, if it represents a regular profile.
        if (otrProfileId == null) return null;

        return otrProfileId.toString();
    }

    /**
     * Construct OtrProfileId from the string representation.
     *
     * @param value The string representation of the OtrProfileId that is generated with {@link
     *     OtrProfileId#serialize} function.
     * @param profile The {@link Profile} used to verify that the off-the-record profile exists. If
     *     this is an off-the-record profile, its original profile is checked.
     * @return An OtrProfileId instance, or null if {@code value} is null or empty.
     * @throws IllegalStateException when the OTR profile belonging to the OtrProfileId is not
     *     available on {@code profile}. The off-the-record profile should exist when OtrProfileId
     *     will be used.
     */
    public static @Nullable OtrProfileId deserializeAndVerify(
            @Nullable String value, Profile profile) {
        assert profile != null;
        OtrProfileId otrProfileId = deserializeWithoutVerify(value);

        // The off-the-record profile should exist for the given OtrProfileId, since OtrProfileId
        // creation is completed just before the profile creation. So there should always be a
        // profile for the id. If OTR profile is not available, deserializeAndVerify should not be
        // called.
        if (otrProfileId != null && !profile.hasOffTheRecordProfile(otrProfileId)) {
            throw new IllegalStateException("The OTR profile should exist for otr profile id.");
        }

        return otrProfileId;
    }

    /**
     * Construct OtrProfileId from the string representation. It is possible this {@link
     * OtrProfileId} does not correspond to a real profile.
     *
     * @param value The string representation of the OtrProfileId that is generated with {@link
     *     OtrProfileId#serialize} function.
     * @return An OtrProfileId instance.
     */
    @CalledByNative
    public static @Nullable OtrProfileId deserializeWithoutVerify(
            @JniType("std::string") @Nullable String value) {
        // The value might be null, if it represents a regular profile.
        if (TextUtils.isEmpty(value)) return null;

        // Check if the format is align with |OtrProfileId#toString| function.
        assert value.startsWith("OtrProfileId{") && value.endsWith("}");

        // Be careful when changing here. This should be consistent with |OtrProfileId#toString|
        // function.
        String id = value.substring("OtrProfileId{".length(), value.length() - 1);
        OtrProfileId otrProfileId = new OtrProfileId(id);

        return otrProfileId;
    }

    public boolean isPrimaryOtrId() {
        return this.equals(sPrimaryOtrProfileId);
    }

    /** Returns true if this id was created by {@link #createUniqueIncognitoCctId()}. */
    public boolean isIncognitoCCId() {
        return OtrProfileIdJni.get().isIncognitoCctId(mProfileId);
    }

    /**
     * @return The OtrProfileId of the primary off-the-record profile. Note that each regular
     *     profile has its own primary off-the-record profile using this same ID.
     */
    public static OtrProfileId getPrimaryOtrProfileId() {
        return sPrimaryOtrProfileId;
    }

    /**
     * Returns true for id of primary and non-primary off-the-record profiles. Otherwise returns
     * false.
     *
     * @param profileId The OtrProfileId
     * @return Whether given OtrProfileId belongs to a off-the-record profile.
     */
    public static boolean isOffTheRecord(@Nullable OtrProfileId profileId) {
        return profileId != null;
    }

    /**
     * Checks whether the given OtrProfileIds are equal. Note that two equal OtrProfileIds (or two
     * nulls) only represent the same profile if they belong to the same original regular profile.
     *
     * @param otrProfileId1 The first OtrProfileId
     * @param otrProfileId2 The second OtrProfileId
     * @return Whether the given OtrProfileIds are equals.
     */
    public static boolean areEqual(
            @Nullable OtrProfileId otrProfileId1, @Nullable OtrProfileId otrProfileId2) {
        // If both OtrProfileIds are null, then both represent a regular profile.
        if (otrProfileId1 == null) return otrProfileId2 == null;

        return otrProfileId1.equals(otrProfileId2);
    }

    /**
     * Checks whether the given OtrProfileId strings are equal. Note that two equal OtrProfileId
     * strings (or two empty/null values) only represent the same profile if they belong to the same
     * original regular profile.
     *
     * @param otrProfileId1 The string of first OtrProfileId
     * @param otrProfileId2 The string of second OtrProfileId
     * @return Whether the given OtrProfileIds are equals.
     */
    public static boolean areEqual(@Nullable String otrProfileId1, @Nullable String otrProfileId2) {
        // If both OtrProfileIds are null or empty, then both represent a regular profile.
        if (TextUtils.isEmpty(otrProfileId1)) {
            return TextUtils.isEmpty(otrProfileId2);
        }

        return otrProfileId1.equals(otrProfileId2);
    }

    @Override
    public String toString() {
        return String.format("OtrProfileId{%s}", mProfileId);
    }

    @Override
    public int hashCode() {
        return mProfileId.hashCode();
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (!(obj instanceof OtrProfileId other)) return false;
        return mProfileId.equals(other.mProfileId);
    }

    @NativeMethods
    public interface Natives {
        OtrProfileId createUniqueOtrProfileId(@JniType("std::string") String profileIdPrefix);

        OtrProfileId getPrimaryId();

        OtrProfileId createUniqueIncognitoCctId();

        boolean isIncognitoCctId(@JniType("std::string") String profileId);
    }
}
