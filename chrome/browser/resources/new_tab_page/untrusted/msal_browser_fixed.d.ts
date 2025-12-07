// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PublicClientApplication as MsalPublicClientApplication} from '@azure/msal-browser/lib/app/PublicClientApplication.d.ts';
import type {Configuration as MsalConfiguration} from '@azure/msal-browser/lib/config/Configuration.d.ts';
import type {PopupRequest as MsalPopupRequest} from '@azure/msal-browser/lib/request/PopupRequest.d.ts';
import type {AuthenticationResult as MsalAuthenticationResult} from '@azure/msal-browser/lib/response/AuthenticationResult.d.ts';
import type {AccountInfo as MsalAccountInfo} from '@azure/msal-common/lib/account/AccountInfo.d.ts';
import type {AuthError as MsalAuthError} from '@azure/msal-common/lib/error/AuthError.d.ts';

declare global {
  export namespace msal {
    export const AccountInfo: MsalAccountInfo;
    export const AuthenticationResult: MsalAuthenticationResult;
    export const AuthError: MsalAuthError;
    export const Configuration: MsalConfiguration;
    export const PopupRequest: MsalPopupRequest;
    export const PublicClientApplication: typeof MsalPublicClientApplication;
  }
}
