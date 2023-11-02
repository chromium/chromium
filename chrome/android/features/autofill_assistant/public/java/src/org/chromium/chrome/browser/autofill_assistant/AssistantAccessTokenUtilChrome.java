// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.accounts.Account;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.autofill_assistant.AssistantAccessTokenUtil;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Implementation of {@link AssistantAccessTokenUtil} for Chrome.
 */
public class AssistantAccessTokenUtilChrome implements AssistantAccessTokenUtil {
    /** OAuth2 scope that RPCs require. */
    private static final String AUTH_TOKEN_TYPE =
            "oauth2:https://www.googleapis.com/auth/userinfo.profile";

    @Override
    public void getAccessToken(Account account, IdentityManager.GetAccessTokenCallback callback) {
        getIdentityManager().getAccessToken(account, AUTH_TOKEN_TYPE, callback);
    }

    @Override
    public void invalidateAccessToken(String accessToken) {
        getIdentityManager().invalidateAccessToken(accessToken);
    }

    private IdentityManager getIdentityManager() {
        return IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
    }
}
