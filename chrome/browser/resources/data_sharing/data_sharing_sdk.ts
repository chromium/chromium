// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dummy implementation of data_sharing_sdk.js for non-branded build.

declare enum Code {
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

declare enum ShareAction {
  COPY_LINK = 0,
  SHARE_1P = 1,
  SHARE_3P = 2,
}

interface DataSharingSdkResponse {
  readonly finalShareAction?: {action: ShareAction, status: Code};
}
declare interface Token {
  access_token: string;
}
declare global {
  interface Window {
    gapi: {auth: {getToken: () => Token}};
    data_sharing_sdk: {
      setOauthAccessToken: (options: {accessToken: string}) => void,
      createGroup: (options: {resourceId: string}) => void,
      readGroups: (options: {resourceIds: string[]}) => void,
      runJoinFlow: (options: {
        resourceId: string,
        tokenSecret: string,
        parent?: HTMLElement,
      }) => Promise<DataSharingSdkResponse>,
      runInviteFlow: (options: {resourceId: string, parent?: HTMLElement}) =>
          Promise<DataSharingSdkResponse>,
      runManageFlow: (options: {resourceId: string, parent?: HTMLElement}) =>
          Promise<DataSharingSdkResponse>,
      updateClearcut: (options: {enabled: boolean}) => void,
    };
  }
}
export {};

window.data_sharing_sdk = {
  setOauthAccessToken: () => void{},
  createGroup: () => void{},
  readGroups: () => void{},
  runJoinFlow: () => Promise.resolve({}),
  runInviteFlow: () => Promise.resolve({}),
  runManageFlow: () => Promise.resolve({}),
  updateClearcut: () => void{},
};
