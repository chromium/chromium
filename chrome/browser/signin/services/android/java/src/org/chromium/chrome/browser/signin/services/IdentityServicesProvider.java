// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Provides access to sign-in related services that are profile-keyed on the native side. Java
 * equivalent of AccountTrackerServiceFactory and similar classes.
 */
public class IdentityServicesProvider {
    private static IdentityServicesProvider sIdentityServicesProvider;

    private IdentityServicesProvider() {}

    public static IdentityServicesProvider get() {
        if (sIdentityServicesProvider == null) {
            sIdentityServicesProvider = new IdentityServicesProvider();
        }
        return sIdentityServicesProvider;
    }

    public static void setInstanceForTests(IdentityServicesProvider provider) {
        var oldValue = sIdentityServicesProvider;
        sIdentityServicesProvider = provider;
        ResettersForTesting.register(() -> sIdentityServicesProvider = oldValue);
    }

    /**
     * Getter for {@link IdentityManager} instance for given profile.
     * @param profile The profile to get regarding identity manager.
     * @return a {@link IdentityManager} instance, or null if the incognito Profile is supplied.
     */
    @MainThread
    public @Nullable IdentityManager getIdentityManager(Profile profile) {
        ThreadUtils.assertOnUiThread();
        IdentityManager result = IdentityServicesProviderJni.get().getIdentityManager(profile);
        return result;
    }

    /**
     * Getter for {@link SigninManager} instance for given profile.
     *
     * @param profile The profile to get regarding sign-in manager.
     * @return a {@link SigninManager} instance, or null if the incognito Profile is supplied.
     */
    @MainThread
    public @Nullable SigninManager getSigninManager(Profile profile) {
        ThreadUtils.assertOnUiThread();
        SigninManager result = IdentityServicesProviderJni.get().getSigninManager(profile);
        return result;
    }

    @NativeMethods
    public interface Natives {
        IdentityManager getIdentityManager(@JniType("Profile*") Profile profile);

        SigninManager getSigninManager(@JniType("Profile*") Profile profile);
    }
}
