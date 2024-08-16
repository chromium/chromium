// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_sharing_sdk.js';

import {BrowserProxyApi} from './browser_proxy_api.js';

let initialized: boolean = false;

const browserProxy: BrowserProxyApi = BrowserProxyApi.getInstance();

browserProxy.callbackRouter.onAccessTokenFetched.addListener(
    (accessToken: string) => {
      window.data_sharing_sdk.setOauthAccessToken({accessToken});
      if (!initialized) {
        browserProxy.handler.apiInitComplete();
        initialized = true;
      }
    },
);
