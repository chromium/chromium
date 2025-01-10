// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MicrosoftAuthUntrustedDocumentProxy} from './microsoft_auth_proxy.js';
import type {AuthenticationResult, AuthError, Configuration, PopupRequest} from './msal_browser.js';
import {PublicClientApplication} from './msal_browser.js';
import type {MicrosoftAuthUntrustedDocumentCallbackRouter} from './ntp_microsoft_auth_shared_ui.mojom-webui.js';

const msalConfig: typeof Configuration = {
  auth: {
    clientId: '',  // TODO(crbug.com/386827067): Insert prod client ID
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
msalApp.initialize().then(() => {
  callbackRouter =
      MicrosoftAuthUntrustedDocumentProxy.getInstance().callbackRouter;
  callbackRouter.acquireTokenPopup.addListener(acquireTokenPopup);
});

// TODO(crbug.com/386389311): Send acquired token to handler.
function handleAcquireTokenResponse(_: typeof AuthenticationResult|null) {}

// TODO(crbug.com/386390859): Send auth error to handler.
function handleAuthError(_: typeof AuthError) {}

async function acquireTokenPopup(): Promise<void> {
  msalApp.acquireTokenPopup(requestConfig)
      .then(handleAcquireTokenResponse)
      .catch(handleAuthError);
}
