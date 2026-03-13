// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Account, JsNetErrorCode, NewScreencastPrecondition, PendingScreencast} from './ash/webui/projector_app/public/mojom/projector_types.mojom-webui.js';

/**
 * Structure for XHR response.
 */
export interface XhrResponseResult {
  success: boolean;
  response?: string;
  error?: string;
  errorCode: JsNetErrorCode;
}

/**
 * Interface to describe a video.
 */
export interface Video {
  thumbnailLink?: string;
  durationMillis?: string;
  fileId?: string;
  srcUrl?: string;
}

/**
 * The delegate interface that the Projector app can use to make requests to
 * chrome.
 */
export interface ClientDelegate {
  getNewScreencastPreconditionState(): Promise<NewScreencastPrecondition>;

  shouldDownloadSoda(): Promise<boolean>;

  installSoda(): Promise<boolean>;

  getPendingScreencasts(): Promise<PendingScreencast[]>;

  getUserPref(userPref: string): Promise<boolean|number>;

  setUserPref(userPref: string, value: boolean|number): Promise<boolean>;

  openFeedbackDialog(): Promise<void>;

  startProjectorSession(storageDir: string): Promise<boolean>;

  sendXhr(
      url: string, method: string, requestBody: string|null,
      useCredentials: boolean, useApiKey: boolean,
      headers: {[key: string]: string}|null,
      accountEmail: string|null): Promise<XhrResponseResult>;

  getAccounts(): Promise<Account[]>;

  getVideo(videoFileId: string, resourceKey: string|null): Promise<Video>;

  getOAuthTokenForAccount(): Promise<Object>;

  onError(msg: string[]): void;
}

/**
 * The client Api for interacting with the Projector app instance.
 */
export interface AppApi {
  setClientDelegate(delegate: ClientDelegate): void;

  onNewScreencastPreconditionChanged(state: NewScreencastPrecondition): void;

  onSodaInstallProgressUpdated(progress: number): void;

  onSodaInstallError(): void;

  onSodaInstalled(): void;

  onScreencastsStateChange(screencasts: PendingScreencast[]): void;
}
