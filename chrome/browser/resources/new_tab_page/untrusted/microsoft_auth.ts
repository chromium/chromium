// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MicrosoftAuthUntrustedDocumentProxy} from './microsoft_auth_proxy.js';
import type {AuthenticationResult, AuthError, Configuration, PopupRequest} from './msal_browser.js';
import {PublicClientApplication} from './msal_browser.js';
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
};

const requestConfig: typeof PopupRequest = {
  scopes: ['Calendars.Read', 'Sites.Read.All'],
};

const msalApp = new PublicClientApplication(msalConfig);
let callbackRouter: MicrosoftAuthUntrustedDocumentCallbackRouter;
let handler: MicrosoftAuthUntrustedPageHandlerRemote;
msalApp.initialize().then(() => {
  callbackRouter =
      MicrosoftAuthUntrustedDocumentProxy.getInstance().callbackRouter;
  callbackRouter.acquireTokenPopup.addListener(acquireTokenPopup);
  handler = MicrosoftAuthUntrustedDocumentProxy.getInstance().handler;
});

function handleAcquireTokenResponse(result: typeof AuthenticationResult|null) {
  if (result && result.expiresOn) {
    handler.setAccessToken(
        {token: result.accessToken, expiration: toTime(result.expiresOn)});
  }
}


function handleAuthError(_: typeof AuthError) {
  // All authentication errors are currently treated the same:
  // the service is marked as errored, which cancels the current
  // authentication attempt and triggers UI updates to prompt
  // the user to retry authenticating.
  handler.setAuthStateError();
}

async function acquireTokenPopup(): Promise<void> {
  msalApp.acquireTokenPopup(requestConfig)
      .then(handleAcquireTokenResponse)
      .catch(handleAuthError);
}
