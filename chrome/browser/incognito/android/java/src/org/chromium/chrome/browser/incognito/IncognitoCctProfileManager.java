// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.annotation.SuppressLint;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

import javax.inject.Inject;

/**
 * This class that takes care of creating and destroying the Incognito profile
 * associated to an Incognito CCT via {@link CustomTabIncognitoManager}.
 *
 * It also implements {@link UnownedUserData} to attach to the Activity's {@link WindowAndroid}
 * which can be used to query the underlying Incognito Profile via
 * {@link IncognitoUtils#getNonPrimaryOTRProfileFromWindowAndroid(WindowAndroid)}
 */
@ActivityScope
public class IncognitoCctProfileManager implements UnownedUserData {
    @SuppressLint("StaticFieldLeak") // This is for test only.
    private static @Nullable IncognitoCctProfileManager sIncognitoCctProfileManagerForTesting;

    /** The key for accessing this object on an {@link org.chromium.base.UnownedUserDataHost}. */
    private static final UnownedUserDataKey<IncognitoCctProfileManager> KEY =
            new UnownedUserDataKey<>(IncognitoCctProfileManager.class);

    private OTRProfileID mOTRProfileID;
    private final WindowAndroid mWindowAndroid;

    @Inject
    public IncognitoCctProfileManager(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        attach(mWindowAndroid, this);
    }

    /**
     * Make this instance of IncognitoCCTProfileManager available through the activity's window.
     * @param window A {@link WindowAndroid} to attach to.
     * @param manager The {@link IncognitoCctProfileManager} to attach.
     */
    private static void attach(WindowAndroid window, IncognitoCctProfileManager manager) {
        KEY.attachToHost(window.getUnownedUserDataHost(), manager);
    }

    /**
     * Detach the provided IncognitoCCTProfileManager from any host it is associated with.
     * @param manager The {@link IncognitoCctProfileManager} to detach.
     */
    private static void detach(IncognitoCctProfileManager manager) {
        KEY.detachFromAllHosts(manager);
    }

    /**
     * Get the Activity's {@link IncognitoCctProfileManager} from the provided {@link
     * WindowAndroid}.
     * @param window The window to get the manager from.
     * @return The Activity's {@link IncognitoCctProfileManager}.
     */
    public static @Nullable IncognitoCctProfileManager from(WindowAndroid window) {
        if (sIncognitoCctProfileManagerForTesting != null) {
            return sIncognitoCctProfileManagerForTesting;
        }

        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    public static void setIncognitoCctProfileManagerForTesting(
            IncognitoCctProfileManager incognitoCctProfileManager) {
        sIncognitoCctProfileManagerForTesting = incognitoCctProfileManager;
        ResettersForTesting.register(() -> sIncognitoCctProfileManagerForTesting = null);
    }

    public Profile getProfile() {
        if (mOTRProfileID == null) mOTRProfileID = OTRProfileID.createUnique("CCT:Incognito");
        return Profile.getLastUsedRegularProfile().getOffTheRecordProfile(
                mOTRProfileID, /*createIfNeeded=*/true);
    }

    public void destroyProfile() {
        if (mOTRProfileID != null) {
            Profile.getLastUsedRegularProfile()
                    .getOffTheRecordProfile(mOTRProfileID, /*createIfNeeded=*/true)
                    .destroyWhenAppropriate();
            mOTRProfileID = null;
        }
        detach(this);
    }
}
