// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dummy implementation of data_sharing_sdk.js for non-branded build.
import type {AddAccessTokenParams, AddAccessTokenResult, AddMemberParams, CreateGroupParams, CreateGroupResult, DataSharingSdk, DataSharingSdkGroupData, DataSharingSdkResponse, DeleteGroupParams, LeaveGroupParams, ReadGroupOptions, ReadGroupParams, ReadGroupResult, ReadGroupsParams, ReadGroupsResult, RunCloseFlowParams, RunDeleteFlowParams, RunInviteFlowParams, RunJoinFlowParams, RunManageFlowParams} from './data_sharing_sdk_types.js';
import {Code} from './data_sharing_sdk_types.js';

// Add something to the dialog to tell which flow it is.
function appendTextForTesting(text: string) {
  const newDiv: HTMLElement = document.createElement('div');
  newDiv.textContent = text;
  document.body.appendChild(newDiv);
}

export function buildDataSharingSdk() {
  return DataSharingSdkImpl.getInstance();
}

window.data_sharing_sdk = {
  buildDataSharingSdk,
};

const groupMemberMapFunction =
    (param: ReadGroupParams): DataSharingSdkGroupData => ({
      groupId: param.groupId,
      displayName: 'GROUP_NAME',
      members: [
        {
          focusObfuscatedGaiaId: 'GAIA_ID',
          displayName: 'MEMBER_NAME',
          email: 'test@gmail.com',
          role: 'member',
          avatarUrl: 'http://example.com',
          givenName: 'MEMBER_NAME',
        },
      ],
      formerMembers: [
        {
          focusObfuscatedGaiaId: 'GAIA_ID2',
          displayName: 'MEMBER_NAME2',
          email: 'test2@gmail.com',
          role: 'former_member',
          avatarUrl: 'http://example2.com',
          givenName: 'MEMBER_NAME2',
        },
      ],
    });

export class DataSharingSdkImpl implements DataSharingSdk {
  createGroup(
      _params: CreateGroupParams,
      ): Promise<{result?: CreateGroupResult, status: Code}> {
    return Promise.resolve({
      result: {groupData: {groupId: '', members: [], formerMembers: []}},
      status: Code.OK,
    });
  }
  readGroup(_param: ReadGroupParams, _options: ReadGroupOptions):
      Promise<{result?: ReadGroupResult, status: Code}> {
    return new Promise((resolve) => {
      resolve({
        status: Code.OK,
        result: {groupData: groupMemberMapFunction(_param)},
      });
    });
  }
  readGroups(
      _params: ReadGroupsParams,
      ): Promise<{result?: ReadGroupsResult, status: Code}> {
    return new Promise((resolve) => {
      resolve({
        status: Code.OK,
        result: {
          groupData: _params.params.map(groupMemberMapFunction),
        },
      });
    });
  }
  addMember(_params: AddMemberParams): Promise<{status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }
  deleteGroup(_params: DeleteGroupParams): Promise<{status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }
  leaveGroup(_params: LeaveGroupParams): Promise<{status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }
  addAccessToken(
      _params: AddAccessTokenParams,
      ): Promise<{result?: AddAccessTokenResult, status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }

  runJoinFlow(_params: RunJoinFlowParams): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake join dialog');
    return new Promise(() => {});
  }
  runInviteFlow(_params: RunInviteFlowParams): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake invite dialog');
    return new Promise(() => {});
  }
  runManageFlow(_params: RunManageFlowParams): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake manage dialog');
    return new Promise(() => {});
  }
  runCloseFlow(_params: RunCloseFlowParams): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake close dialog');
    return new Promise(() => {});
  }
  runDeleteFlow(_params: RunDeleteFlowParams): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake delete dialog');
    return new Promise(() => {});
  }

  // Setup Helpers
  setOauthAccessToken(_params: {accessToken: string}): void {}
  updateClearcut(_params: {enabled: boolean}): void {}

  static getInstance(): DataSharingSdk {
    return dataSharingSdkInstance ||
        (dataSharingSdkInstance = new DataSharingSdkImpl());
  }

  static setInstance(obj: DataSharingSdk) {
    dataSharingSdkInstance = obj;
  }
}

let dataSharingSdkInstance: DataSharingSdk|null = null;
