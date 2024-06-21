// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

/** Wrapper that allows passing a OTRProfileID reference around in the Java layer. */
public class OTRProfileID {
    private final String mProfileID;
    // OTRProfileID value should be same with Profile::OTRProfileID::PrimaryID in native.
    private static final OTRProfileID sPrimaryOTRProfileID =
            new OTRProfileID("profile::primary_otr");
    private static final String INCOGNITO_CCT_OTR_PROFILE_ID_PREFIX = "CCT:Incognito";

    @CalledByNative
    public OTRProfileID(String profileID) {
        assert profileID != null;
        mProfileID = profileID;
    }

    @CalledByNative
    private String getProfileID() {
        return mProfileID;
    }

    /** Creates a unique profile id by appending a unique serial number to the given prefix. */
    public static OTRProfileID createUnique(String profileIDPrefix) {
        return OTRProfileIDJni.get().createUniqueOTRProfileID(profileIDPrefix);
    }

    /**
     * Creates a new unique Incognito CCT profile id by appending a unique serial number to the iCCT
     * OTR profile id prefix {@link #INCOGNITO_CCT_OTR_PROFILE_ID_PREFIX}.
     */
    public static OTRProfileID createUniqueIncognitoCCTId() {
        return OTRProfileIDJni.get().createUniqueOTRProfileID(INCOGNITO_CCT_OTR_PROFILE_ID_PREFIX);
    }

    /**
     * Deconstruct OTRProfileID to a string representation. Deconstructed version will be used to
     * pass the id to the native side and /components/ layer.
     *
     * @param otrProfileID An OTRProfileId instance.
     * @return A string that represents the given otrProfileID.
     */
    @CalledByNative
    public static String serialize(OTRProfileID otrProfileID) {
        // The OTRProfileID might be null, if it represents the regular profile.
        if (otrProfileID == null) return null;

        return otrProfileID.toString();
    }

    /**
     * Construct OTRProfileID from the string representation.
     *
     * @param value The string representation of the OTRProfileID that is generated with {@link
     *         OTRProfileID#serialize} function.
     * @return An OTRProfileID instance.
     * @throws IllegalStateException when the OTR profile belongs to the OTRProfileID is not
     *         available. The off-the-record profile should exist when OTRProfileId will be used.
     */
    public static OTRProfileID deserialize(String value) {
        OTRProfileID otrProfileId = deserializeWithoutVerify(value);

        // The off-the-record profile should exist for the given OTRProfileID, since OTRProfileID
        // creation is completed just before the profile creation. So there should always be a
        // profile for the id. If OTR profile is not available, deserialize function should not be
        // called.
        if (otrProfileId != null
                && !ProfileManager.getLastUsedRegularProfile()
                        .hasOffTheRecordProfile(otrProfileId)) {
            throw new IllegalStateException("The OTR profile should exist for otr profile id.");
        }

        return otrProfileId;
    }

    /**
     * Construct OTRProfileID from the string representation. It is possible this {@link
     * OTRProfileID} does not correspond to a real profile.
     * @param value The string representation of the OTRProfileID that is generated with {@link
     *        OTRProfileID#serialize} function.
     * @return An OTRProfileID instance.
     */
    @CalledByNative
    public static OTRProfileID deserializeWithoutVerify(String value) {
        // The value might be null, if it represents the regular profile.
        if (TextUtils.isEmpty(value)) return null;

        // Check if the format is align with |OTRProfileID#toString| function.
        assert value.startsWith("OTRProfileID{") && value.endsWith("}");

        // Be careful when changing here. This should be consistent with |OTRProfileID#toString|
        // function.
        String id = value.substring("OTRProfileID{".length(), value.length() - 1);
        OTRProfileID otrProfileId = new OTRProfileID(id);

        return otrProfileId;
    }

    public boolean isPrimaryOTRId() {
        return this.equals(sPrimaryOTRProfileID);
    }

    /**
     * Returns true if the OTR profile id starts with {@link #INCOGNITO_CCT_OTR_PROFILE_ID_PREFIX}.
     */
    public boolean isIncognitoCCId() {
        return this.getProfileID().startsWith(INCOGNITO_CCT_OTR_PROFILE_ID_PREFIX);
    }

    /**
     * @return The OTRProfileID of the primary off-the-record profile.
     */
    public static OTRProfileID getPrimaryOTRProfileID() {
        return sPrimaryOTRProfileID;
    }

    /**
     * Returns true for id of primary and non-primary off-the-record profiles. Otherwise returns
     * false.
     * @param profileID The OTRProfileID
     * @return Whether given OTRProfileID belongs to a off-the-record profile.
     */
    public static boolean isOffTheRecord(@Nullable OTRProfileID profileID) {
        return profileID != null;
    }

    /**
     * Checks whether the given OTRProfileIDs belong to the same profile.
     * @param otrProfileID1 The first OTRProfileID
     * @param otrProfileID2 The second OTRProfileID
     * @return Whether the given OTRProfileIDs are equals.
     */
    public static boolean areEqual(
            @Nullable OTRProfileID otrProfileID1, @Nullable OTRProfileID otrProfileID2) {
        // If both OTRProfileIDs null, then both belong to the regular profile.
        if (otrProfileID1 == null) return otrProfileID2 == null;

        return otrProfileID1.equals(otrProfileID2);
    }

    /**
     * Checks whether the given OTRProfileID strings belong to the same profile.
     * @param otrProfileID1 The string of first OTRProfileID
     * @param otrProfileID2 The string of second OTRProfileID
     * @return Whether the given OTRProfileIDs are equals.
     */
    public static boolean areEqual(@Nullable String otrProfileID1, @Nullable String otrProfileID2) {
        // If both OTRProfileIDs null, then both belong to the regular profile.
        if (TextUtils.isEmpty(otrProfileID1)) {
            return TextUtils.isEmpty(otrProfileID2);
        }

        return otrProfileID1.equals(otrProfileID2);
    }

    @Override
    public String toString() {
        return String.format("OTRProfileID{%s}", mProfileID);
    }

    @Override
    public int hashCode() {
        return mProfileID.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof OTRProfileID)) return false;
        OTRProfileID other = (OTRProfileID) obj;
        return mProfileID.equals(other.mProfileID);
    }

    @NativeMethods
    public interface Natives {
        OTRProfileID createUniqueOTRProfileID(String profileIDPrefix);

        OTRProfileID getPrimaryID();
    }
}
