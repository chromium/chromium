// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_sharing_sdk.js';

import {BrowserProxy} from './browser_proxy.js';

let initialized: boolean = false;

function onDOMContentLoaded() {
  const browserProxy: BrowserProxy = BrowserProxy.getInstance();
  browserProxy.callbackRouter.onAccessTokenFetched.addListener(
      (accessToken: string) => {
        window.data_sharing_sdk.setOauthAccessToken({accessToken});
        if (!initialized) {
          initialize();
          browserProxy.handler.showUI();
          initialized = true;
        }
      },
  );
}

function getResourceId() {
  const resourceIdElement = document.getElementById(
                                'resource-id',
                                ) as HTMLInputElement |
      null;
  return resourceIdElement?.value;
}

function getTokenSecret() {
  const tokenSecretElement = document.getElementById(
                                 'token-secret',
                                 ) as HTMLInputElement |
      null;
  return tokenSecretElement?.value;
}

function maybeRunIfResourceId(
    runFunction: (options: {resourceId: string}) => void,
) {
  const resourceId = getResourceId();
  if (resourceId) {
    runFunction({resourceId});
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
    maybeRunIfResourceId(
        (options: {resourceId: string}) => window.data_sharing_sdk.runJoinFlow({
          resourceId: options.resourceId,
          tokenSecret: getTokenSecret() || '',
        }),
    );
  });

  const inviteFlowButton = document.getElementById('create-invite-flow-button');
  inviteFlowButton?.addEventListener('click', () => {
    maybeRunIfResourceId(window.data_sharing_sdk.runInviteFlow);
  });

  const manageFlowButton = document.getElementById('create-manage-flow-button');
  manageFlowButton?.addEventListener('click', () => {
    maybeRunIfResourceId(window.data_sharing_sdk.runManageFlow);
  });

  const createGroupButton = document.getElementById('create-group-button');
  createGroupButton?.addEventListener('click', () => {
    maybeRunIfResourceId(window.data_sharing_sdk.createGroup);
  });
}

document.addEventListener('DOMContentLoaded', onDOMContentLoaded);
