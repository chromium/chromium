// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';

// </if>

import type {DataSharingMemberRole, DataSharingSdkGroupData, DataSharingSdkGroupMember} from './data_sharing_sdk_types.js';
import type {GroupData, GroupMember} from './group_data.mojom-webui.js';
import {MemberRole} from './group_data.mojom-webui.js';

// Utilities to convert DataSharingSdkGroup in data sharing sdk to GroupData in
// group_data.mojom
export function toMojomGroupData(group: DataSharingSdkGroupData): GroupData {
  const members: GroupMember[] = [];
  for (const member of group.members) {
    members.push(toMojomGroupMember(member));
  }
  return {
    groupId: group.groupId,
    displayName: group.displayName || '',
    accessToken: group.accessToken || '',
    members,
  };
}

function toMojomGroupMember(member: DataSharingSdkGroupMember): GroupMember {
  return {
    gaiaId: member.focusObfuscatedGaiaId,
    displayName: member.displayName,
    email: member.email,
    role: toMojomRole(member.role),
    avatarUrl: {url: member.avatarUrl},
    givenName: member.givenName,
  };
}

function toMojomRole(role: DataSharingMemberRole): MemberRole {
  // Applicant will eventually be added, it's supported by the Data Sharing SDK.
  switch (role) {
    case 'invitee':
      return MemberRole.kInvitee;
    case 'member':
      return MemberRole.kMember;
    case 'owner':
      return MemberRole.kOwner;
    default:
      return MemberRole.kUnspecified;
  }
}
