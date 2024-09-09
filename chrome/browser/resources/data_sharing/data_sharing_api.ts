// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';

// </if>

import {BrowserProxy} from './browser_proxy.js';
import type {GroupData} from './group_data.mojom-webui.js';
import {toMojomGroupData} from './mojom_conversion_utils.js';


let initialized: boolean = false;

const browserProxy: BrowserProxy = BrowserProxy.getInstance();

browserProxy.callbackRouter.onAccessTokenFetched.addListener(
    (accessToken: string) => {
      window.data_sharing_sdk.setOauthAccessToken({accessToken});
      if (!initialized) {
        browserProxy.handler.apiInitComplete();
        initialized = true;
      }
    },
);

browserProxy.callbackRouter.readGroups.addListener((groupIds: string[]) => {
  return new Promise((resolve) => {
    window.data_sharing_sdk.readGroups({groupIds})
        .then(
            (groups) => {
              const groupData: GroupData[] = [];
              for (const group of groups) {
                groupData.push(toMojomGroupData(group));
              }
              resolve({groups: groupData});
            },
            (err) => {
              console.error(err);
              throw err;
            });
  });
});
