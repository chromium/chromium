// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const enum Code {
  OK = 0,
  CANCELLED = 1,
  UNKNOWN = 2,
  INVALID_ARGUMENT = 3,
  DEADLINE_EXCEEDED = 4,
  NOT_FOUND = 5,
  ALREADY_EXISTS = 6,
  PERMISSION_DENIED = 7,
  UNAUTHENTICATED = 16,
  RESOURCE_EXHAUSTED = 8,
  FAILED_PRECONDITION = 9,
  ABORTED = 10,
  OUT_OF_RANGE = 11,
  UNIMPLEMENTED = 12,
  INTERNAL = 13,
  UNAVAILABLE = 14,
  DATA_LOSS = 15,
  DO_NOT_USE_RESERVED_FOR_FUTURE_EXPANSION_USE_DEFAULT_IN_SWITCH_INSTEAD = 20,
}

export const enum LearnMoreUrlType {
  LEARN_MORE_URL_TYPE_UNSPECIFIED = 0,
  PEOPLE_WITH_ACCESS_SUBTITLE = 1,
  DESCRIPTION_INVITE = 2,
  DESCRIPTION_JOIN = 3,
  BLOCK = 4,
}

export const enum ShareAction {
  COPY_LINK,
  SHARE_1P,
  SHARE_3P,
}

export declare interface DataSharingSdkResponse {
  result?: {shareAction?: ShareAction, groupId?: string, tokenSecret?: string};
  readonly status: Code;
}

export declare interface DataSharingSdkSitePreview {
  url: string;
  faviconUrl?: string;
}

export declare interface DataSharingSdkGetLinkParams {
  groupId: string;
  tokenSecret?: string;
}

export declare type DataSharingSdkGetLink = (
    params: DataSharingSdkGetLinkParams,
    ) => Promise<string>;

export declare interface Token {
  access_token: string;
}

export declare interface DataSharingSdkGroupId {
  groupId?: string;
}

export declare interface DataSharingSdkGroupData {
  groupId: string;
  members: DataSharingSdkGroupMember[];
  displayName?: string;
  accessToken?: string;
}

export declare type DataSharingMemberRole =
    | 'unknown' | 'member' | 'owner' | 'invitee';

export declare interface DataSharingSdkGroupMember {
  focusObfuscatedGaiaId: string;
  displayName: string;
  email: string;
  role: DataSharingMemberRole;
  avatarUrl: string;
  givenName: string;
}

export declare interface CreateGroupParams {
  displayName: string;
}

export declare interface CreateGroupResult {
  groupData: DataSharingSdkGroupData;
}

export declare interface ReadGroupsParams {
  groupIds: string[];
}

export declare interface ReadGroupsResult {
  groupData: DataSharingSdkGroupData[];
}

export declare interface AddMemberParams {
  groupId: string;
  accessToken?: string;
}

export declare interface RemoveMemberParams {
  groupId: string;
  memberFocusObfuscatedGaiaId: string;
}

export declare interface DeleteGroupParams {
  groupId: string;
}

export declare interface AddAccessTokenParams {
  groupId: string;
}

export declare interface AddAccessTokenResult {
  groupData: DataSharingSdkGroupData;
}

export declare interface DataSharingSdk {
  createGroup(
      params: CreateGroupParams,
      ): Promise<{result?: CreateGroupResult, status: Code}>;
  readGroups(
      params: ReadGroupsParams,
      ): Promise<{result?: ReadGroupsResult, status: Code}>;
  addMember(params: AddMemberParams): Promise<{status: Code}>;
  removeMember(params: RemoveMemberParams): Promise<{status: Code}>;
  deleteGroup(params: DeleteGroupParams): Promise<{status: Code}>;
  addAccessToken(
      params: AddAccessTokenParams,
      ): Promise<{result?: AddAccessTokenResult, status: Code}>;

  runJoinFlow(
      params: DataSharingSdkGroupId&{
        tokenSecret: string,
        parent?: HTMLElement,
        previewSites?: DataSharingSdkSitePreview[],
        learnMoreUrlMap?: {[type in LearnMoreUrlType]?: () => string},
      },
      ): Promise<DataSharingSdkResponse>;
  runInviteFlow(params: {
    parent?: HTMLElement,
    getShareLink?: DataSharingSdkGetLink,
    title?: string,
    learnMoreUrlMap?: {[type in LearnMoreUrlType]?: () => string},
  }): Promise<DataSharingSdkResponse>;
  runManageFlow(
      params: DataSharingSdkGroupId&{
        parent?: HTMLElement,
        getShareLink?: DataSharingSdkGetLink,
        learnMoreUrlMap?: {[type in LearnMoreUrlType]?: () => string},
      },
      ): Promise<DataSharingSdkResponse>;

  setOauthAccessToken(params: {accessToken: string}): void;
  updateClearcut(params: {enabled: boolean}): void;
}

declare global {
  interface Window {
    gapi: {auth: {getToken: () => Token}};
    data_sharing_sdk: {buildDataSharingSdk: () => DataSharingSdk};
  }
}
