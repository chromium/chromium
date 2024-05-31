// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.content_settings.CookieControlsEnforcement;

/**
 * Communicates between CookieControlsService (C++ backend) and observers in the Incognito NTP Java
 * UI.
 */
public class CookieControlsServiceBridge {
    /**
     * Interface for a class that wants to receive cookie controls updates from
     * CookieControlsServiceBridge.
     */
    public interface CookieControlsServiceObserver {
        /**
         * Called when there is an update in the cookie controls that should be reflected in the UI.
         * @param checked A boolean indicating whether the toggle indicating third-party cookies are
         *         currently being blocked should be checked or not.
         * @param enforcement A CookieControlsEnforcement enum type indicating the enforcement rule
         *         for these cookie controls.
         */
        public void sendCookieControlsUIChanges(
                boolean checked, @CookieControlsEnforcement int enforcement);
    }

    private long mNativeCookieControlsServiceBridge;
    private CookieControlsServiceObserver mObserver;

    /**
     * Initializes a CookieControlsServiceBridge instance.
     *
     * @param profile The {@link Profile} associated with the cookie controls.
     * @param observer An observer to call with updates from the cookie controls service.
     */
    public CookieControlsServiceBridge(Profile profile, CookieControlsServiceObserver observer) {
        mObserver = observer;
        mNativeCookieControlsServiceBridge =
                CookieControlsServiceBridgeJni.get()
                        .init(CookieControlsServiceBridge.this, profile);
    }

    /** Destroys the native counterpart of this class. */
    public void destroy() {
        if (mNativeCookieControlsServiceBridge != 0) {
            CookieControlsServiceBridgeJni.get()
                    .destroy(mNativeCookieControlsServiceBridge, CookieControlsServiceBridge.this);
            mNativeCookieControlsServiceBridge = 0;
        }
    }

    /**
     * Updates the CookieControlsService on the status of the toggle, and thus the state of
     * third-party cookie blocking in incognito.
     * @param enable A boolean indicating whether the toggle has been switched on or off.
     */
    public void handleCookieControlsToggleChanged(boolean enable) {
        CookieControlsServiceBridgeJni.get()
                .handleCookieControlsToggleChanged(mNativeCookieControlsServiceBridge, enable);
    }

    /** Starts a service to observe current profile. */
    public void updateServiceIfNecessary() {
        CookieControlsServiceBridgeJni.get()
                .updateServiceIfNecessary(mNativeCookieControlsServiceBridge);
    }

    @CalledByNative
    private void sendCookieControlsUIChanges(
            boolean checked, @CookieControlsEnforcement int enforcement) {
        mObserver.sendCookieControlsUIChanges(checked, enforcement);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        long init(CookieControlsServiceBridge caller, @JniType("Profile*") Profile profile);

        void destroy(long nativeCookieControlsServiceBridge, CookieControlsServiceBridge caller);

        void handleCookieControlsToggleChanged(
                long nativeCookieControlsServiceBridge, boolean enable);

        void updateServiceIfNecessary(long nativeCookieControlsServiceBridge);
    }
}
