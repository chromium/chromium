// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.components.content_settings.ContentSettingsType;

/** Java equivalent of unused_site_permissions_bridge.cc. */
class UnusedSitePermissionsBridge {
    interface Observer {
        void revokedPermissionsChanged();
    }

    private static ProfileKeyedMap<UnusedSitePermissionsBridge> sProfileMap;

    private final Profile mProfile;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    static UnusedSitePermissionsBridge getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileMap.getForProfile(profile, UnusedSitePermissionsBridge::new);
    }

    UnusedSitePermissionsBridge(Profile profile) {
        mProfile = profile;
    }

    void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    PermissionsData[] getRevokedPermissions() {
        return UnusedSitePermissionsBridgeJni.get().getRevokedPermissions(mProfile);
    }

    void regrantPermissions(String origin) {
        UnusedSitePermissionsBridgeJni.get().regrantPermissions(mProfile, origin);
        notifyRevokedPermissionsChanged();
    }

    void undoRegrantPermissions(PermissionsData permissionsData) {
        UnusedSitePermissionsBridgeJni.get().undoRegrantPermissions(mProfile, permissionsData);
        notifyRevokedPermissionsChanged();
    }

    void clearRevokedPermissionsReviewList() {
        UnusedSitePermissionsBridgeJni.get().clearRevokedPermissionsReviewList(mProfile);
        notifyRevokedPermissionsChanged();
    }

    void restoreRevokedPermissionsReviewList(PermissionsData[] permissionsDataList) {
        UnusedSitePermissionsBridgeJni.get()
                .restoreRevokedPermissionsReviewList(mProfile, permissionsDataList);
        notifyRevokedPermissionsChanged();
    }

    static String[] contentSettingsTypeToString(
            @ContentSettingsType.EnumType int[] contentSettingsTypeList) {
        return UnusedSitePermissionsBridgeJni.get()
                .contentSettingsTypeToString(contentSettingsTypeList);
    }

    private void notifyRevokedPermissionsChanged() {
        for (Observer observer : mObservers) {
            observer.revokedPermissionsChanged();
        }
    }

    @NativeMethods
    interface Natives {
        @JniType("std::vector<PermissionsData>")
        PermissionsData[] getRevokedPermissions(@JniType("Profile*") Profile profile);

        void regrantPermissions(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void undoRegrantPermissions(
                @JniType("Profile*") Profile profile,
                @JniType("PermissionsData") PermissionsData permissionsData);

        void clearRevokedPermissionsReviewList(@JniType("Profile*") Profile profile);

        void restoreRevokedPermissionsReviewList(
                @JniType("Profile*") Profile profile,
                @JniType("std::vector<PermissionsData>") PermissionsData[] permissionsDataList);

        @JniType("std::vector<std::u16string>")
        String[] contentSettingsTypeToString(
                @JniType("std::vector<std::int32_t>") int[] contentSettingsTypeList);
    }
}
