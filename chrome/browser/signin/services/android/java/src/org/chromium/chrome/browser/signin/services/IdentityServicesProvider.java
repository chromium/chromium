// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.MainThread;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Provides access to sign-in related services that are profile-keyed on the native side. Java
 * equivalent of AccountTrackerServiceFactory and similar classes.
 */
@NullMarked
public class IdentityServicesProvider {
    private static @Nullable IdentityServicesProvider sIdentityServicesProvider;
    private static @Nullable IdentityManager sIdentityManager;
    private static @Nullable SigninManager sSigninManager;
    private static @Nullable AccountPreviewDataService sAccountPreviewDataServiceForTesting;

    private IdentityServicesProvider() {}

    public static IdentityServicesProvider get() {
        if (sIdentityServicesProvider == null) {
            sIdentityServicesProvider = new IdentityServicesProvider();
        }
        return sIdentityServicesProvider;
    }

    /**
     * @deprecated Use {@link #setIdentityManagerForTesting(IdentityManager)} and {@link
     *     #setSigninManagerForTesting(SigninManager)} instead.
     *     <p>TODO(crbug.com/476991282): Remove this method.
     */
    @Deprecated
    public static void setInstanceForTests(IdentityServicesProvider provider) {
        var oldValue = sIdentityServicesProvider;
        sIdentityServicesProvider = provider;
        ResettersForTesting.register(() -> sIdentityServicesProvider = oldValue);
    }

    public static void setIdentityManagerForTesting(IdentityManager identityManager) {
        var oldValue = sIdentityManager;
        sIdentityManager = identityManager;
        ResettersForTesting.register(() -> sIdentityManager = oldValue);
    }

    public static void setSigninManagerForTesting(SigninManager signinManager) {
        var oldValue = sSigninManager;
        sSigninManager = signinManager;
        ResettersForTesting.register(() -> sSigninManager = oldValue);
    }

    public static void setAccountPreviewDataServiceForTesting(
            @Nullable AccountPreviewDataService accountPreviewDataService) {
        var oldValue = sAccountPreviewDataServiceForTesting;
        sAccountPreviewDataServiceForTesting = accountPreviewDataService;
        ResettersForTesting.register(() -> sAccountPreviewDataServiceForTesting = oldValue);
    }

    /**
     * Getter for {@link IdentityManager} instance for given profile.
     *
     * @param profile The profile to get regarding identity manager.
     * @return a {@link IdentityManager} instance, or null if the incognito Profile is supplied.
     */
    @MainThread
    public @Nullable IdentityManager getIdentityManager(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sIdentityManager != null) {
            return sIdentityManager;
        }
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
        if (sSigninManager != null) {
            return sSigninManager;
        }
        SigninManager result = IdentityServicesProviderJni.get().getSigninManager(profile);
        return result;
    }

    /**
     * Getter for {@link AccountPreviewDataService} instance for given profile.
     *
     * @param profile The profile to get regarding account preview data service.
     * @return a {@link AccountPreviewDataService} instance, or null if the incognito Profile is
     *     supplied.
     */
    @MainThread
    public @Nullable AccountPreviewDataService getAccountPreviewDataService(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sAccountPreviewDataServiceForTesting != null) {
            return sAccountPreviewDataServiceForTesting;
        }

        return IdentityServicesProviderJni.get().getAccountPreviewDataService(profile);
    }

    @NativeMethods
    public interface Natives {
        @JniType("signin::IdentityManager*")
        IdentityManager getIdentityManager(@JniType("Profile*") Profile profile);

        SigninManager getSigninManager(@JniType("Profile*") Profile profile);

        @JniType("signin::AccountPreviewDataService*")
        @Nullable AccountPreviewDataService getAccountPreviewDataService(
                @JniType("Profile*") Profile profile);
    }
}
