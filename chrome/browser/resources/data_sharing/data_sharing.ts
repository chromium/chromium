// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';

// </if>


import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {$} from 'chrome-untrusted://resources/js/util.js';

import {BrowserProxy} from './browser_proxy.js';
import {Code} from './data_sharing_sdk_types.js';
import type {DataSharingSdk, DataSharingSdkGetLinkParams} from './data_sharing_sdk_types.js';

// Param names in loaded URL. Should match those in
// chrome/browser/ui/views/data_sharing/data_sharing_utils.cc.
enum UrlQueryParams {
  FLOW = 'flow',
  GROUP_ID = 'group_id',
  TOKEN_SECRET = 'token_secret',
  TAB_GROUP_ID = 'tab_group_id',
}

enum FlowValues {
  SHARE = 'share',
  JOIN = 'join',
  MANAGE = 'manage',
}

let initialized: boolean = false;
let dataSharingSdk: DataSharingSdk;

function onDOMContentLoaded() {
  ColorChangeUpdater.forDocument().start();
  dataSharingSdk = window.data_sharing_sdk.buildDataSharingSdk();
  const browserProxy: BrowserProxy = BrowserProxy.getInstance();
  browserProxy.callbackRouter.onAccessTokenFetched.addListener(
      (accessToken: string) => {
        dataSharingSdk.setOauthAccessToken({accessToken});
        if (!initialized) {
          initialize();
          processUrl(browserProxy);
          browserProxy.handler.showUI();
          initialized = true;
        }
      },
  );
}

// TODO(pengchaocai): Test in follow up CLs.
function processUrl(browserProxy: BrowserProxy) {
  const currentUrl = window.location.href;
  const params = new URL(currentUrl).searchParams;
  const flow = params.get(UrlQueryParams.FLOW);
  const groupId = params.get(UrlQueryParams.GROUP_ID);
  const tokenSecret = params.get(UrlQueryParams.TOKEN_SECRET);
  const tabGroupId = params.get(UrlQueryParams.TAB_GROUP_ID);

  // Called with when the owner presses copy link in share dialog.
  function makeTabGroupShared(
      options: {groupId: string, tokenSecret?: string}) {
    browserProxy.handler.associateTabGroupWithGroupId(
        tabGroupId!, options.groupId);
  }

  function getShareLink(params: {groupId: string, tokenSecret?: string}):
      Promise<string> {
    return browserProxy.handler
        .getShareLink(params.groupId, params.tokenSecret!)
        .then(res => res.url.url);
  }

  switch (flow) {
    case FlowValues.SHARE:
      dataSharingSdk.runInviteFlow({
        getShareLink: (params: DataSharingSdkGetLinkParams):
            Promise<string> => {
              makeTabGroupShared(params);
              return getShareLink(params);
            },
      });
      break;
    case FlowValues.JOIN:
      // group_id and token_secret cannot be null for join flow.
      dataSharingSdk.runJoinFlow(
          {groupId: groupId!, tokenSecret: tokenSecret!});
      break;
    case FlowValues.MANAGE:
      // group_id cannot be null for manage flow.
      dataSharingSdk.runManageFlow({
        groupId: groupId!,
        getShareLink: (params: DataSharingSdkGetLinkParams):
            Promise<string> => {
              return getShareLink(params);
            },
      });
      break;
    default:
      const debugContainer: HTMLDivElement|null =
          $('debug-container') as HTMLDivElement;
      debugContainer.setAttribute('debug', 'true');
      break;
  }
}

function getGroupId() {
  const groupIdElement = document.getElementById(
                             'group-id',
                             ) as HTMLInputElement |
      null;
  return groupIdElement?.value;
}

function getTokenSecret() {
  const tokenSecretElement = document.getElementById(
                                 'token-secret',
                                 ) as HTMLInputElement |
      null;
  return tokenSecretElement?.value;
}

function maybeRunIfGroupId(
    runFunction: (options: {groupId: string}) => void,
) {
  const id = getGroupId();
  if (id) {
    runFunction({groupId: id});
  }
}

export function setOauthAccessToken(accessToken: string) {
  dataSharingSdk.setOauthAccessToken({accessToken});
}

async function initialize() {
  const joinFlowButton = document.getElementById(
      'create-join-flow-button',
  );
  joinFlowButton?.addEventListener('click', () => {
    maybeRunIfGroupId(
        (options: {groupId: string}) => dataSharingSdk.runJoinFlow({
          groupId: options.groupId,
          tokenSecret: getTokenSecret() || '',
        }),
    );
  });

  const inviteFlowButton = document.getElementById('create-invite-flow-button');
  inviteFlowButton?.addEventListener('click', () => {
    dataSharingSdk.runInviteFlow({});
  });

  const manageFlowButton = document.getElementById('create-manage-flow-button');
  manageFlowButton?.addEventListener('click', () => {
    maybeRunIfGroupId(dataSharingSdk.runManageFlow);
  });

  const groupIdElement = document.getElementById(
                             'group-id',
                             ) as HTMLInputElement |
      null;
  // Ease of testing
  const existingGroupId = localStorage.getItem('group-id');
  if (existingGroupId && groupIdElement) {
    groupIdElement.value = existingGroupId;
  }

  const createGroupButton = document.getElementById('create-group-button');
  createGroupButton?.addEventListener('click', () => {
    dataSharingSdk
        .createGroup(/* options= */ {displayName: 'Test Display Name'})
        .then(
            ({result, status}) => {
              if (status === Code.OK) {
                const groupData = result?.groupData;
                console.info(groupData);
                if (groupIdElement && groupData) {
                  // Ease of testing
                  groupIdElement.value = groupData.groupId;
                  localStorage.setItem('group-id', groupData.groupId);
                }
              } else {
                console.error(status);
                throw status;
              }
            },
            // Catchall for errors Data Sharing SDK doesn't catch.
            (err) => {
              console.error(err);
              throw err;
            });
  });

  const readGroupButton = document.getElementById('read-group-button');
  readGroupButton?.addEventListener('click', () => {
    maybeRunIfGroupId((params) => {
      dataSharingSdk.readGroups({groupIds: [params.groupId]})
          .then(
              ({result, status}) => {
                if (status === Code.OK) {
                  const groupData = result?.groupData;
                  console.info(groupData);
                } else {
                  console.error(status);
                  throw status;
                }
              },
              // Catchall for errors Data Sharing SDK doesn't catch.
              (err) => {
                console.error(err);
                throw err;
              });
    });
  });
}

document.addEventListener('DOMContentLoaded', onDOMContentLoaded);
