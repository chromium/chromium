// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {GroupData, GroupMember} from './data_sharing_internals.mojom-webui.js';
import {RoleType} from './data_sharing_internals.mojom-webui.js';
import {DataSharingInternalsBrowserProxy} from './data_sharing_internals_browser_proxy.js';

function getProxy(): DataSharingInternalsBrowserProxy {
  return DataSharingInternalsBrowserProxy.getInstance();
}

function appendTextChildToList(textToShow: string, element: HTMLUListElement) {
  const textElement = document.createElement('li');
  textElement.textContent = textToShow;
  element.appendChild(textElement);
}

function roleTypeToString(role: RoleType): string {
  switch (role) {
    case RoleType.UNKNOWN:
      return 'Unknown';
    case RoleType.OWNER:
      return 'Owner';
    case RoleType.MEMBER:
      return 'Member';
    case RoleType.INVITEE:
      return 'Invitee';
  }
}

function addMemberToGroup(member: GroupMember, group: HTMLUListElement) {
  const memberlistItem = document.createElement('li');
  const memberItem = document.createElement('ul');
  appendTextChildToList(member.displayName, memberItem);
  appendTextChildToList(member.email, memberItem);
  appendTextChildToList(roleTypeToString(member.role), memberItem);
  appendTextChildToList(member.avatarUrl.url, memberItem);
  memberlistItem.appendChild(memberItem);
  group.appendChild(memberlistItem);
}


/**
 * Show all groups information.
 */
function displayGroups(isSuccess: boolean, groupData: GroupData[]) {
  getRequiredElement('get-all-groups-status').textContent =
      isSuccess ? 'success' : 'failed';
  const groupList = getRequiredElement('group-list');
  groupData.forEach((group) => {
    const listItem = document.createElement('li');
    const groupItem = document.createElement('ul');
    appendTextChildToList(group.groupId, groupItem);
    appendTextChildToList(group.name, groupItem);
    group.members.forEach((member) => {
      addMemberToGroup(member, groupItem);
    });
    listItem.appendChild(groupItem);
    groupList.appendChild(listItem);
  });
}

function initialize() {
  getProxy().getAllGroups().then(
      response => displayGroups(response.isSuccess, response.data));
}

document.addEventListener('DOMContentLoaded', initialize);
