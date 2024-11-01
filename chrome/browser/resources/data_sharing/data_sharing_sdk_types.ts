// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IMPORTANT: This file should be kept in sync with
// third_party/data_sharing_sdk/data_sharing_sdk_types.ts
// Only update this file by copying the content from that file and fix the
// formatting.
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
  status: Code;
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
export declare interface LeaveGroupParams {
  groupId: string;
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
export declare interface RunJoinFlowParams extends DataSharingSdkGroupId {
  tokenSecret: string;
  parent: HTMLElement;
  learnMoreUrlMap: {[type in LearnMoreUrlType]?: () => string};
  onJoinSuccessful: () => void;
  fetchPreviewData: () => Promise<DataSharingSdkSitePreview[]>;
}
export declare interface RunInviteFlowParams {
  parent: HTMLElement;
  getShareLink: DataSharingSdkGetLink;
  groupName: string;
  learnMoreUrlMap: {[type in LearnMoreUrlType]?: () => string};
}
export declare interface RunManageFlowParams extends DataSharingSdkGroupId {
  parent: HTMLElement;
  getShareLink: DataSharingSdkGetLink;
  learnMoreUrlMap: {[type in LearnMoreUrlType]?: () => string};
}
export declare interface DataSharingSdk {
  createGroup(
      params: CreateGroupParams,
      ): Promise<{result?: CreateGroupResult, status: Code}>;
  readGroups(
      params: ReadGroupsParams,
      ): Promise<{result?: ReadGroupsResult, status: Code}>;
  addMember(params: AddMemberParams): Promise<{status: Code}>;
  leaveGroup(params: LeaveGroupParams): Promise<{status: Code}>;
  deleteGroup(params: DeleteGroupParams): Promise<{status: Code}>;
  addAccessToken(
      params: AddAccessTokenParams,
      ): Promise<{result?: AddAccessTokenResult, status: Code}>;
  runJoinFlow(params: RunJoinFlowParams): Promise<DataSharingSdkResponse>;
  runInviteFlow(params: RunInviteFlowParams): Promise<DataSharingSdkResponse>;
  runManageFlow(params: RunManageFlowParams): Promise<DataSharingSdkResponse>;
  setOauthAccessToken(params: {accessToken: string}): void;
  updateClearcut(params: {enabled: boolean}): void;
}
declare global {
  interface Window {
    gapi: {auth: {getToken: () => Token}};
    data_sharing_sdk: {buildDataSharingSdk(): DataSharingSdk};
  }
}
