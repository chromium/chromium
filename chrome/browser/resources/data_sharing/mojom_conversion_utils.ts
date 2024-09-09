// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './data_sharing_sdk.js';
// </if>
// <if expr="not _google_chrome">
import './dummy_data_sharing_sdk.js';

// </if>

import type {GroupData, GroupMember} from './group_data.mojom-webui.js';
import {MemberRole} from './group_data.mojom-webui.js';

// Utilities to convert DataSharingSdkGroup in data sharing sdk to GroupData in
// group_data.mojom
// TODO(crbug.com/362524829): Replace any with types once they are exported from
// data sharing sdk.
export function toMojomGroupData(group: any): GroupData {
  const members: GroupMember[] = [];
  for (const member of group.members) {
    members.push(toMojomGroupMember(member));
  }
  return {
    groupId: group.id,
    displayName: group.name || '',
    // Fetching access token is not yet supported for the API.
    accessToken: '',
    members,
  };
}

function toMojomGroupMember(member: any): GroupMember {
  return {
    gaiaId: member.profileId,
    displayName: member.displayName,
    email: member.displayValue,
    role: toMojomRole(member.role),
    avatarUrl: {url: member.photoUrl},
  };
}

function toMojomRole(role: any): MemberRole {
  // 'applicant' is not yet supported for the API.
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
