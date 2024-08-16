// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Java equivalent of magic_stack_bridge.cc */
class MagicStackBridge {
    interface Observer {
        void activeModuleDismissed();
    }

    private static ProfileKeyedMap<MagicStackBridge> sProfileMap;

    private final Profile mProfile;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    static MagicStackBridge getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileMap.getForProfile(profile, MagicStackBridge::new);
    }

    MagicStackBridge(Profile profile) {
        mProfile = profile;
    }

    @Nullable
    MagicStackEntry getModuleToShow() {
        return MagicStackBridgeJni.get().getModuleToShow(mProfile);
    }

    void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    void notifyObservers() {
        for (Observer observer : mObservers) {
            observer.activeModuleDismissed();
        }
    }

    void dismissActiveModule() {
        MagicStackBridgeJni.get().dismissActiveModule(mProfile);
        notifyObservers();
    }

    void dismissSafeBrowsingModule() {
        MagicStackBridgeJni.get().dismissSafeBrowsingModule(mProfile);
    }

    void dismissCompromisedPasswordsModule() {
        MagicStackBridgeJni.get().dismissCompromisedPasswordsModule(mProfile);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::optional<MenuNotificationEntry>")
        @Nullable
        MagicStackEntry getModuleToShow(@JniType("Profile*") Profile profile);

        void dismissActiveModule(@JniType("Profile*") Profile profile);

        void dismissSafeBrowsingModule(@JniType("Profile*") Profile profile);

        void dismissCompromisedPasswordsModule(@JniType("Profile*") Profile profile);
    }
}
