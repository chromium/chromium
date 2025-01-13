// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';
// </if>

import '/strings.m.js';

import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {ReadGroupsParams as MojomReadGroupsParams} from './data_sharing.mojom-webui.js';
import type {DataSharingSdk, ReadGroupParams} from './data_sharing_sdk_types.js';
import type {GroupData} from './group_data.mojom-webui.js';
import {toMojomGroupData} from './mojom_conversion_utils.js';

let initialized: boolean = false;
const dataSharingSdk: DataSharingSdk =
    window.data_sharing_sdk.buildDataSharingSdk();

const browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

dataSharingSdk.updateClearcut(
    {enabled: loadTimeData.getBoolean('metricsReportingEnabled')});
browserProxy.callbackRouter.onAccessTokenFetched.addListener(
    (accessToken: string) => {
      dataSharingSdk.setOauthAccessToken({accessToken});
      if (!initialized) {
        browserProxy.handler!.apiInitComplete();
        initialized = true;
      }
    },
);

browserProxy.callbackRouter.readGroups.addListener(
    (mojomParams: MojomReadGroupsParams) => {
      const params: ReadGroupParams[] = [];
      for (const mojomParam of mojomParams.params) {
        params.push({
          groupId: mojomParam.groupId,
          consistencyToken: mojomParam.consistencyToken,
        });
      }
      return new Promise((resolve) => {
        dataSharingSdk.readGroups({params}).then(({result, status}) => {
          const groupData: GroupData[] =
              result?.groupData.map(toMojomGroupData) ?? [];
          resolve({result: {groups: groupData, statusCode: status}});
        });
      });
    });

browserProxy.callbackRouter.leaveGroup.addListener((groupId: string) => {
  return new Promise((resolve) => {
    dataSharingSdk.leaveGroup({groupId}).then(({status}) => {
      resolve({statusCode: status});
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
