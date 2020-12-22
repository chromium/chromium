// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Wrapper that allows passing a OTRProfileID reference around in the Java layer.
 */
public class OTRProfileID {
    private final String mProfileID;

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
     * Deconstruct OTRProfileID to a string representation.
     * Deconstructed version will be used to pass the id to the native side and /components/ layer.
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
        // The value might be null, if it represents the regular profile.
        if (value == null || value.isEmpty()) return null;

        // Check if the format is align with |OTRProfileID#toString| function.
        assert value.startsWith("OTRProfileID{") && value.endsWith("}");

        // Be careful when changing here. This should be consistent with |OTRProfileID#toString|
        // function.
        String id = value.substring("OTRProfileID{".length(), value.length() - 1);
        OTRProfileID otrProfileID = new OTRProfileID(id);

        // The off-the-record profile should exist for the given OTRProfileID, since OTRProfileID
        // creation is completed just before the profile creation. So there should always be a
        // profile for the id. If OTR profile is not available, deserialize function should not be
        // called.
        if (!Profile.getLastUsedRegularProfile().hasOffTheRecordProfile(otrProfileID)) {
            throw new IllegalStateException("The OTR profile should exist for otr profile id.");
        }

        return otrProfileID;
    }

    public boolean isPrimaryOTRId() {
        OTRProfileID primaryId = OTRProfileIDJni.get().getPrimaryID();
        return this.equals(primaryId);
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
