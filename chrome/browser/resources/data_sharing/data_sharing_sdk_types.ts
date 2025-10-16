// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IMPORTANT: This file should be kept in sync with
// third_party/data_sharing_sdk/data_sharing_sdk_types.ts
// Only update this file by copying the content from that file and fix the
// formatting.
/* eslint-disable */
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
  SAVE_CHANGES_DIALOG = 5,
  GLOBAL_ACCESS = 6,
}
export const enum ShareAction {
  COPY_LINK,
  SHARE_1P,
  SHARE_3P,
}
export declare interface DataSharingSdkResponse {
  result?: {shareAction?: ShareAction; groupId?: string; tokenSecret?: string;};
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
export const enum ShareKitFlowType {
  JOIN = 'join',
  INVITE = 'invite',
  MANAGE = 'manage',
}
export declare interface DisplayedUserData {
  name?: string;
  email?: string;
}
export declare interface DynamicMessageParams {
  displayedUser?: DisplayedUserData;
  group: {
    name: string;
    members: Array<{name: string; email: string; role: DataSharingMemberRole}>;
  };
  loggedInUser: {name: string; email: string; role: DataSharingMemberRole;};
  payload: {title: string; description: string; mediaCount: number;};
}
export const enum StaticMessageKey {

  CANCEL_LABEL,
  CANCEL,
  CLOSE,
  LOADING,
  BACK,
  ERROR_DIALOG_CONTENT,
  SOMETHING_WENT_WRONG,
  FAIL_TO_UPDATE_ACCESS,
  THERE_WAS_AN_ERROR,
  THERE_WAS_AN_ISSUE,
  MORE_OPTIONS,
  MORE_OPTIONS_DESCRIPTION,

  INVITE_FLOW_DESCRIPTION_CONTENT,
  COPY_INVITE_LINK,
  COPY_LINK,
  LEARN_ABOUT_SHARED_TAB_GROUPS,
  LINK_COPY_SUCCESS,
  LINK_COPY_FAILED,

  JOIN_AND_OPEN_LABEL,
  LEARN_MORE_JOIN_FLOW,
  TAB_GROUP_DETAILS,
  COLLECTION_LIST_TITLE,

  ANYONE_WITH_LINK_TOGGLE_TITLE,
  ANYONE_WITH_LINK_TOGGLE_DESCRIPTION,
  BLOCK_AND_LEAVE_GROUP,
  BLOCK_AND_LEAVE,
  LEARN_ABOUT_BLOCKED_ACCOUNTS,
  GOT_IT,
  ABUSE_BANNER_CONTENT,
  STOP_SHARING_DIALOG_TITLE,
  STOP_SHARING,
  BLOCK,
  LEAVE_GROUP,
  LEAVE,
  LEAVE_GROUP_DIALOG_TITLE,
  REMOVE,
  YOU,
  OWNER,
  PEOPLE_WITH_ACCESS,
  PEOPLE_WITH_ACCESS_SUBTITLE_MANAGE_FLOW,
  GROUP_FULL_TITLE,
  GROUP_FULL_CONTENT,
  YOUR_GROUP_IS_FULL_DESCRIPTION,
  SHARING_DISABLED_DESCRIPTION,
  ACTIVITY_LOGS,

  CLOSE_FLOW_HEADER,
  KEEP_GROUP,
  DELETE_GROUP,

  DELETE_FLOW_HEADER,
  DELETE,
}
export const enum DynamicMessageKey {

  GET_INVITE_FLOW_HEADER,
  GET_MEMBERSHIP_PREVIEW_OWNER_LABEL,

  GET_JOIN_FLOW_DESCRIPTION_HEADER,
  GET_JOIN_FLOW_DESCRIPTION_CONTENT,
  GET_MEMBERSHIP_PREVIEW_INVITEE_LABEL,
  GET_GROUP_PREVIEW_MEMBER_DESCRIPTION,
  GET_GROUP_PREVIEW_TAB_DESCRIPTION,
  GET_GROUP_PREVIEW_ARIA_LABEL,

  GET_MANAGE_FLOW_HEADER,
  GET_STOP_SHARING_DIALOG_CONTENT,
  GET_REMOVE_DIALOG_TITLE,
  GET_REMOVE_DIALOG_CONTENT,
  GET_LEAVE_GROUP_DIALOG_CONTENT,
  GET_BLOCK_DIALOG_TITLE,
  GET_BLOCK_DIALOG_CONTENT,
  GET_BLOCK_AND_LEAVE_DIALOG_CONTENT,

  GET_CLOSE_FLOW_DESCRIPTION_FIRST_PARAGRAPH,
  GET_CLOSE_FLOW_DESCRIPTION_SECOND_PARAGRAPH,

  GET_DELETE_FLOW_DESCRIPTION_CONTENT,
}
export declare interface TranslationMap {
  static: {[key in StaticMessageKey]: string};
  dynamic:
      {[key in DynamicMessageKey]: (params: DynamicMessageParams) => string;};
}
export declare interface DataSharingSdkGroupData {
  groupId: string;
  members: DataSharingSdkGroupMember[];
  formerMembers: DataSharingSdkGroupMember[];
  displayName?: string;
  accessToken?: string;
  consistencyToken?: string;
  serializedCollaborationGroupMetadata?: string;
}
export declare type DataSharingMemberRole =
    | 'unknown' | 'member' | 'owner' | 'invitee' | 'former_member';
export const enum DataSharingMemberRoleEnum {
  UNKNOWN = 'unknown',
  MEMBER = 'member',
  OWNER = 'owner',
  INVITEE = 'invitee',
  FORMER_MEMBER = 'former_member',
}
export declare interface DataSharingSdkGroupMember {
  focusObfuscatedGaiaId: string;
  displayName: string;
  email: string;
  role: DataSharingMemberRole;
  avatarUrl: string;
  givenName: string;
  createdAtTimeMs: number;
  lastUpdatedAtTimeMs: number;
}
export declare interface CreateGroupParams {
  displayName: string;
  serializedCollaborationGroupMetadata?: string;
}
export declare interface CreateGroupResult {
  groupData: DataSharingSdkGroupData;
}
export declare interface ReadGroupParams {
  groupId: string;
  consistencyToken?: string;
}
export declare interface ReadGroupOptions {
  accessToken?: string;
}
export declare interface ReadGroupResult {
  groupData: DataSharingSdkGroupData;
}
export declare interface ReadGroupsParams {
  params: ReadGroupParams[];
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
export const enum LoggingIntent {
  UNKNOWN = 0,
  STOP_SHARING = 1,
  LEAVE_GROUP = 2,
  REMOVE_ACCESS = 3,
  UPDATE_ACCESS = 4,
  BLOCK_USER = 5,
  REMOVE_USER = 6,
  REMOVE_ACCESS_TOKEN = 7,
  ADD_ACCESS_TOKEN = 8,
  COPY_LINK = 9,
  BLOCK_AND_LEAVE = 10,
  OPEN_GROUP_DETAILS = 11,
  OPEN_LEARN_MORE_URL = 12,
  ACCEPT_JOIN_AND_OPEN = 13,
  ABANDON_JOIN = 14,
  KEEP_GROUP = 15,
  DELETE_GROUP = 16,
}
export const enum Progress {
  UNKNOWN = 0,
  STARTED = 1,
  FAILED = 2,
  SUCCEEDED = 3,
}
export declare interface LoggingEvent {
  intentType: LoggingIntent;

  progress: Progress;
}
export declare interface Logger {
  onEvent(event: LoggingEvent): void;
}
export declare interface RunJoinFlowParams extends DataSharingSdkGroupId {
  tokenSecret: string;
  parent: HTMLElement;
  translatedMessages: TranslationMap;
  learnMoreUrlMap: {[type in LearnMoreUrlType]?: () => string};
  onJoinSuccessful: () => void | Promise<void>;
  fetchPreviewData: () => Promise<DataSharingSdkSitePreview[]>;
  logger?: Logger;
}
export declare interface RunInviteFlowParams {
  parent: HTMLElement;
  getShareLink: DataSharingSdkGetLink;
  groupName: string;
  translatedMessages: TranslationMap;
  learnMoreUrlMap: {[type in LearnMoreUrlType]?: () => string};
  logger?: Logger;
  serializedCollaborationGroupMetadata?: string;
}
export declare interface RunManageFlowParams extends DataSharingSdkGroupId {
  parent: HTMLElement;
  getShareLink: DataSharingSdkGetLink;
  translatedMessages: TranslationMap;
  learnMoreUrlMap: {[type in LearnMoreUrlType]?: () => string};
  activityLogCallback?: () => void;
  logger?: Logger;
  showLeaveDialogAtStartup?: boolean;
  isSharingDisabled?: boolean;
}
export declare interface RunCloseFlowParams extends DataSharingSdkGroupId {
  parent: HTMLElement;
  translatedMessages: TranslationMap;
  logger?: Logger;
}
export declare interface RunDeleteFlowParams extends DataSharingSdkGroupId {
  parent: HTMLElement;
  translatedMessages: TranslationMap;
  logger?: Logger;
}
export declare interface DataSharingSdk {
  createGroup(
      params: CreateGroupParams,
      ): Promise<{result?: CreateGroupResult; status: Code}>;
  readGroup(
      params: ReadGroupParams,
      options?: ReadGroupOptions,
      ): Promise<{result?: ReadGroupResult; status: Code}>;
  readGroups(
      params: ReadGroupsParams,
      ): Promise<{result?: ReadGroupsResult; status: Code}>;
  addMember(params: AddMemberParams): Promise<{status: Code}>;
  leaveGroup(params: LeaveGroupParams): Promise<{status: Code}>;
  deleteGroup(params: DeleteGroupParams): Promise<{status: Code}>;
  addAccessToken(
      params: AddAccessTokenParams,
      ): Promise<{result?: AddAccessTokenResult; status: Code}>;
  runJoinFlow(params: RunJoinFlowParams): Promise<DataSharingSdkResponse>;
  runInviteFlow(params: RunInviteFlowParams): Promise<DataSharingSdkResponse>;
  runManageFlow(params: RunManageFlowParams): Promise<DataSharingSdkResponse>;
  runCloseFlow(params: RunCloseFlowParams): Promise<DataSharingSdkResponse>;
  runDeleteFlow(params: RunDeleteFlowParams): Promise<DataSharingSdkResponse>;
  setOauthAccessToken(params: {accessToken: string}): void;
  updateClearcut(params: {enabled: boolean}): void;
  setClientVersionAndResetPeopleStore(
      versionString: string,
      baselineCl: number,
      ): void;
}
declare global {
  interface Window {
    gapi: {auth: {getToken: () => Token}};
    data_sharing_sdk: {buildDataSharingSdk(): DataSharingSdk;};
  }
}
