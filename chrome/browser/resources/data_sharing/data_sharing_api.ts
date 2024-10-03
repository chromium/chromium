// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';

// </if>

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {DataSharingSdk} from './data_sharing_sdk_types.js';
import type {GroupData} from './group_data.mojom-webui.js';
import {toMojomGroupData} from './mojom_conversion_utils.js';

let initialized: boolean = false;
const dataSharingSdk: DataSharingSdk =
    window.data_sharing_sdk.buildDataSharingSdk();

const browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

browserProxy.callbackRouter.onAccessTokenFetched.addListener(
    (accessToken: string) => {
      dataSharingSdk.setOauthAccessToken({accessToken});
      if (!initialized) {
        browserProxy.handler!.apiInitComplete();
        initialized = true;
      }
    },
);

browserProxy.callbackRouter.readGroups.addListener((groupIds: string[]) => {
  return new Promise((resolve) => {
    dataSharingSdk.readGroups({groupIds})
        .then(
            ({result, status}) => {
              const groupData: GroupData[] =
                  result?.groupData.map(toMojomGroupData) ?? [];
              resolve({result: {groups: groupData, statusCode: status}});
            });
  });
});

browserProxy.callbackRouter.deleteGroup.addListener((groupId: string) => {
  return new Promise((resolve) => {
    dataSharingSdk.deleteGroup({groupId}).then(({status}) => {
      resolve({statusCode: status});
    });
  });
});
