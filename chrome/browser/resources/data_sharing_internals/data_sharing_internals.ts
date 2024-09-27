// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import {DataSharingInternalsBrowserProxy} from './data_sharing_internals_browser_proxy.js';
import type {GroupData, GroupMember} from './group_data.mojom-webui.js';
import {MemberRole} from './group_data.mojom-webui.js';

function getProxy(): DataSharingInternalsBrowserProxy {
  return DataSharingInternalsBrowserProxy.getInstance();
}

function appendTextChildToList(textToShow: string, element: HTMLUListElement) {
  const textElement = document.createElement('li');
  textElement.textContent = textToShow;
  element.appendChild(textElement);
}

function roleTypeToString(role: MemberRole): string {
  switch (role) {
    case MemberRole.kUnspecified:
      return 'Unknown';
    case MemberRole.kOwner:
      return 'Owner';
    case MemberRole.kMember:
      return 'Member';
    case MemberRole.kInvitee:
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
  appendTextChildToList(member.givenName, memberItem);
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
    appendTextChildToList(group.displayName, groupItem);
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
