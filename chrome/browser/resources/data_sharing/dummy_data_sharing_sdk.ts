// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dummy implementation of data_sharing_sdk.js for non-branded build.
import type {AddAccessTokenParams, AddAccessTokenResult, AddMemberParams, CreateGroupParams, CreateGroupResult, DataSharingSdk, DataSharingSdkGetLink, DataSharingSdkGroupId, DataSharingSdkResponse, DataSharingSdkSitePreview, DeleteGroupParams, LearnMoreUrlType, ReadGroupsParams, ReadGroupsResult, RemoveMemberParams} from './data_sharing_sdk_types.js';
import {Code} from './data_sharing_sdk_types.js';

// Add something to the dialog to tell which flow it is.
function appendTextForTesting(text: string) {
  const newDiv: HTMLDivElement = document.createElement('div');
  newDiv.textContent = text;
  document.body.appendChild(newDiv);
}

export function buildDataSharingSdk() {
  return new DataSharingSdkImpl();
}

window.data_sharing_sdk = {
  buildDataSharingSdk,
};

class DataSharingSdkImpl implements DataSharingSdk {
  createGroup(
      _params: CreateGroupParams,
      ): Promise<{result?: CreateGroupResult, status: Code}> {
    return Promise.resolve(
        {result: {groupData: {groupId: '', members: []}}, status: Code.OK});
  }
  readGroups(
      _params: ReadGroupsParams,
      ): Promise<{result?: ReadGroupsResult, status: Code}> {
    return new Promise((resolve) => {
      resolve({
        status: Code.OK,
        result: {
          groupData:
              _params.groupIds!.map(groupId => ({
                                      groupId,
                                      displayName: 'GROUP_NAME',
                                      members: [
                                        {
                                          focusObfuscatedGaiaId: 'GAIA_ID',
                                          displayName: 'MEMBER_NAME',
                                          email: 'test@gmail.com',
                                          role: 'member',
                                          avatarUrl: 'http://example.com',
                                        },
                                      ],
                                    })),
        },
      });
    });
  }
  addMember(_params: AddMemberParams): Promise<{status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }
  removeMember(_params: RemoveMemberParams): Promise<{status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }
  deleteGroup(_params: DeleteGroupParams): Promise<{status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }
  addAccessToken(
      _params: AddAccessTokenParams,
      ): Promise<{result?: AddAccessTokenResult, status: Code}> {
    return Promise.resolve({status: Code.UNIMPLEMENTED});
  }

  runJoinFlow(
      _params: DataSharingSdkGroupId&{
        tokenSecret: string,
        parent?: HTMLElement,
        previewSites?: DataSharingSdkSitePreview[],
        learnMoreUrlMap?: {[type in LearnMoreUrlType]?: () => string},
      },
      ): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake join dialog');
    return Promise.resolve({});
  }
  runInviteFlow(_params: {
    parent?: HTMLElement,
    getShareLink?: DataSharingSdkGetLink,
    title?: string,
    learnMoreUrlMap?: {[type in LearnMoreUrlType]?: () => string},
  }): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake invite dialog');
    return Promise.resolve({});
  }
  runManageFlow(
      _params: DataSharingSdkGroupId&{
        parent?: HTMLElement,
        getShareLink?: DataSharingSdkGetLink,
        learnMoreUrlMap?: {[type in LearnMoreUrlType]?: () => string},
      },
      ): Promise<DataSharingSdkResponse> {
    appendTextForTesting('A fake manage dialog');
    return Promise.resolve({});
  }

  // Setup Helpers
  setOauthAccessToken(_params: {accessToken: string}): void {}
  updateClearcut(_params: {enabled: boolean}): void {}
}
