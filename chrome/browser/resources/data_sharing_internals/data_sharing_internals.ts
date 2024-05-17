// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {GroupData} from './data_sharing_internals.mojom-webui.js';
import {DataSharingInternalsBrowserProxy} from './data_sharing_internals_browser_proxy.js';

function getProxy(): DataSharingInternalsBrowserProxy {
  return DataSharingInternalsBrowserProxy.getInstance();
}

function appendTextChildToList(textToShow: string, element: HTMLUListElement) {
  const textElement = document.createElement('pre');
  textElement.textContent = textToShow;
  element.appendChild(textElement);
}

/**
 * Show all groups information.
 */
function displayGroups(isSuccess: boolean, groupData: GroupData[]) {
  getRequiredElement('get-all-groups-status').textContent =
      isSuccess ? 'success' : 'failed';
  const groupList = getRequiredElement('group-list');
  groupData.forEach((group) => {
    const listItem = document.createElement('ul');
    appendTextChildToList(group.groupId, listItem);
    appendTextChildToList(group.name, listItem);
    groupList.appendChild(listItem);
    // TODO (qinmin): display member information for the group.
  });
}

function initialize() {
  getProxy().getAllGroups().then(
      response => displayGroups(response.isSuccess, response.data));
}

document.addEventListener('DOMContentLoaded', initialize);
