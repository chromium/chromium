// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dummy implementation of data_sharing_sdk.js for non-branded build.

declare interface Token {
  access_token: string;
}
declare global {
  interface Window {
    gapi: {auth: {getToken: () => Token}};
    data_sharing_sdk: {
      setOauthAccessToken: (accessToken: string) => void,
      createGroup: (resourceId: string) => void,
      runJoinFlow: (resourceId: string, tokenSecret: string) => void,
      runInviteFlow: (resourceId: string) => void,
      runManageFlow: (resourceId: string) => void,
    };
  }
}
export {};

window.data_sharing_sdk = {
  setOauthAccessToken: () => void{},
  createGroup: () => void{},
  runJoinFlow: () => void{},
  runInviteFlow: () => void{},
  runManageFlow: () => void{},
};
