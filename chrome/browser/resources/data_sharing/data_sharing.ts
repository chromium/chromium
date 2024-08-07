// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_sharing_sdk.js';

import {$} from 'chrome-untrusted://resources/js/util.js';

import {BrowserProxy} from './browser_proxy.js';

// Match 'flow' param from the loaded URL to invoke corresponding dialog.
const FLOW = {
  share: 'share',
  join: 'join',
  manage: 'manage',
};
Object.freeze(FLOW);

let initialized: boolean = false;

function onDOMContentLoaded() {
  const browserProxy: BrowserProxy = BrowserProxy.getInstance();
  browserProxy.callbackRouter.onAccessTokenFetched.addListener(
      (accessToken: string) => {
        window.data_sharing_sdk.setOauthAccessToken({accessToken});
        if (!initialized) {
          initialize();
          processUrl();
          browserProxy.handler.showUI();
          initialized = true;
        }
      },
  );
}

// TODO(pengchaocai): Test in follow up CLs.
async function processUrl() {
  const currentUrl = window.location.href;
  const params = new URL(currentUrl).searchParams;
  const flow = params.get('flow');
  const groupId = params.get('group_id');
  const tokenSecret = params.get('token_secret');

  switch (flow) {
    case FLOW.share:
      await window.data_sharing_sdk.runInviteFlow({});
      break;
    case FLOW.join:
      // group_id and token_secret cannot be null for join flow.
      await window.data_sharing_sdk.runJoinFlow(
          {groupId: groupId!, tokenSecret: tokenSecret!});
      break;
    case FLOW.manage:
      // group_id cannot be null for manage flow.
      await window.data_sharing_sdk.runManageFlow({groupId: groupId!});
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
