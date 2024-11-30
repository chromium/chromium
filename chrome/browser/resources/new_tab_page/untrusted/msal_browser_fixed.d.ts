// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PublicClientApplication as MsalPublicClientApplication} from '@azure/msal-browser/lib/app/PublicClientApplication.d.ts';

declare global {
  export namespace msal {
    export const PublicClientApplication: typeof MsalPublicClientApplication;
  }
}
