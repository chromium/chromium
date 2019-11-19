// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Provides access to sign-in related services that are profile-keyed on the native side. Java
 * equivalent of AccountTrackerServiceFactory and similar classes.
 */
public final class IdentityServicesProvider {
    /** Getter for {@link IdentityManager} instance. */
    public static IdentityManager getIdentityManager() {
        ThreadUtils.assertOnUiThread();
        IdentityManager result =
                IdentityServicesProviderJni.get().getIdentityManager(Profile.getLastUsedProfile());
        assert result != null;
        return result;
    }

    /** Getter for {@link AccountTrackerService} instance. */
    public static AccountTrackerService getAccountTrackerService() {
        ThreadUtils.assertOnUiThread();
        AccountTrackerService result = IdentityServicesProviderJni.get().getAccountTrackerService(
                Profile.getLastUsedProfile());
        assert result != null;
        return result;
    }

    public static SigninManager getSigninManager() {
        ThreadUtils.assertOnUiThread();
        SigninManager result =
                IdentityServicesProviderJni.get().getSigninManager(Profile.getLastUsedProfile());
        assert result != null;
        return result;
    }

    @NativeMethods
    interface Natives {
        public IdentityManager getIdentityManager(Profile profile);
        public AccountTrackerService getAccountTrackerService(Profile profile);
        public SigninManager getSigninManager(Profile profile);
    }
}
