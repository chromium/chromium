// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_sharing_sdk.js';

import {$} from 'chrome-untrusted://resources/js/util.js';

import {BrowserProxy} from './browser_proxy.js';

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

function onDOMContentLoaded() {
  const browserProxy: BrowserProxy = BrowserProxy.getInstance();
  browserProxy.callbackRouter.onAccessTokenFetched.addListener(
      (accessToken: string) => {
        window.data_sharing_sdk.setOauthAccessToken({accessToken});
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
      window.data_sharing_sdk.runInviteFlow({
        getShareLink: (options: {groupId: string, tokenSecret?: string}):
            Promise<string> => {
              makeTabGroupShared(options);
              return getShareLink(options);
            },
      });
      break;
    case FlowValues.JOIN:
      // group_id and token_secret cannot be null for join flow.
      window.data_sharing_sdk.runJoinFlow(
          {groupId: groupId!, tokenSecret: tokenSecret!});
      break;
    case FlowValues.MANAGE:
      // group_id cannot be null for manage flow.
      window.data_sharing_sdk.runManageFlow({
        groupId: groupId!,
        getShareLink: (options: {groupId: string, tokenSecret?: string}):
            Promise<string> => {
              return getShareLink(options);
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
  window.data_sharing_sdk.setOauthAccessToken({accessToken});
}

async function initialize() {
  const joinFlowButton = document.getElementById(
      'create-join-flow-button',
  );
  joinFlowButton?.addEventListener('click', () => {
    maybeRunIfGroupId(
        (options: {groupId: string}) => window.data_sharing_sdk.runJoinFlow({
          groupId: options.groupId,
          tokenSecret: getTokenSecret() || '',
        }),
    );
  });

  const inviteFlowButton = document.getElementById('create-invite-flow-button');
  inviteFlowButton?.addEventListener('click', () => {
    window.data_sharing_sdk.runInviteFlow({});
  });

  const manageFlowButton = document.getElementById('create-manage-flow-button');
  manageFlowButton?.addEventListener('click', () => {
    maybeRunIfGroupId(window.data_sharing_sdk.runManageFlow);
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
    window.data_sharing_sdk.createGroup(/* options= */ {})
        .then(
            (group) => {
              console.info(group);
              if (groupIdElement) {
                // Ease of testing
                groupIdElement.value = group.id;
                localStorage.setItem('group-id', group.id);
              }
            },
            (err) => {
              console.error(err);
              throw err;
            });
  });

  const readGroupButton = document.getElementById('read-group-button');
  readGroupButton?.addEventListener('click', () => {
    maybeRunIfGroupId((params) => {
      window.data_sharing_sdk.readGroups({groupIds: [params.groupId]})
          .then(
              (group) => {
                console.info(group);
              },
              (err) => {
                console.error(err);
                throw err;
              });
    });
  });
}

document.addEventListener('DOMContentLoaded', onDOMContentLoaded);
