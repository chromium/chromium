// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The code in this file interfaces with the Microsoft
 * Authentication Library for its parent New Tab page document.
 */

import {MicrosoftAuthUntrustedDocumentProxy} from './microsoft_auth_proxy.js';
import type {AuthenticationResult, AuthError, Configuration, PopupRequest} from './msal_browser.js';
import {PublicClientApplication} from './msal_browser.js';
import {AuthState} from './ntp_microsoft_auth_shared_ui.mojom-webui.js';
import type {MicrosoftAuthUntrustedDocumentCallbackRouter} from './ntp_microsoft_auth_shared_ui.mojom-webui.js';
import type {MicrosoftAuthUntrustedPageHandlerRemote} from './ntp_microsoft_auth_untrusted_ui.mojom-webui.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;

function toTime(time: Date): {internalValue: bigint} {
  return {
    internalValue: BigInt(time.valueOf()) * 1000n + kWindowsToUnixEpochOffset,
  };
}

const msalConfig: typeof Configuration = {
  auth: {
    clientId: '299cf3a2-777b-4013-8072-c504d2be03a2',
    authority: 'https://login.microsoftonline.com/organizations',
    redirectUri: 'https://chromeenterprise.google/ntp-microsoft-auth',
  },
  cache: {cacheLocation: 'localStorage'},
  system: {iframeHashTimeout: 1500},
};

const requestConfig: typeof PopupRequest = {
  scopes: ['Calendars.Read', 'Sites.Read.All'],
};

const msalApp = new PublicClientApplication(msalConfig);
let callbackRouterToParent: MicrosoftAuthUntrustedDocumentCallbackRouter;
let callbackRouterToHandler: MicrosoftAuthUntrustedDocumentCallbackRouter;
let handler: MicrosoftAuthUntrustedPageHandlerRemote;
msalApp.initialize().then(async () => {
  const proxy = MicrosoftAuthUntrustedDocumentProxy.getInstance();
  callbackRouterToParent = proxy.callbackRouterToParent;
  callbackRouterToHandler = proxy.callbackRouterToHandler;
  handler = proxy.handler;
  callbackRouterToHandler.acquireTokenSilent.addListener(acquireTokenSilent);
  callbackRouterToParent.acquireTokenPopup.addListener(acquireTokenPopup);
  callbackRouterToParent.signOut.addListener(signOut);

  const {state} = await handler.getAuthState();
  if (state === AuthState.kNone) {
    acquireTokenSilent();
  }
});

function handleAcquireTokenResponse(result: typeof AuthenticationResult|null) {
  if (result) {
    // Set the active account even if there's no expiration time,
    // as this indicates that the user has successfully authenticated
    // and we should use this account for future silent authentication
    // attempts.
    if (!msalApp.getActiveAccount() && result.account) {
      msalApp.setActiveAccount(result.account);
    }
    if (result.expiresOn) {
      handler.setAccessToken(
          {token: result.accessToken, expiration: toTime(result.expiresOn)});
    }
  }
}

function handleAuthError(_: typeof AuthError) {
  // All authentication errors are currently treated the same:
  // the service is marked as errored, which cancels the current
  // authentication attempt and triggers UI updates to prompt
  // the user to retry authenticating.
  handler.setAuthStateError();
}

function acquireTokenPopup() {
  msalApp.acquireTokenPopup(requestConfig)
      .then(handleAcquireTokenResponse)
      .catch(handleAuthError);
}

function acquireTokenSilent() {
  msalApp.acquireTokenSilent(requestConfig)
      .then(handleAcquireTokenResponse)
      .catch(handleAuthError);
}

function signOut() {
  msalApp
      .logoutPopup({
        account: msalApp.getActiveAccount(),
        postLogoutRedirectUri:
            'https://chromeenterprise.google/ntp-microsoft-auth',
      })
      .then(() => handler.clearAuthData());
}
