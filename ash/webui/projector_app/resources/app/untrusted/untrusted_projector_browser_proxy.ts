// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UntrustedProjectorPageCallbackRouter, UntrustedProjectorPageHandlerFactory, UntrustedProjectorPageHandlerRemote} from './ash/webui/projector_app/mojom/untrusted_projector.mojom-webui.js';
import {Account, JsNetErrorCode, NewScreencastPrecondition, PendingScreencast, PrefsThatProjectorCanAskFor, RequestType, VideoInfo, XhrResponseCode} from './ash/webui/projector_app/public/mojom/projector_types.mojom-webui.js';

const booleanUserPrefs = new Map<string, PrefsThatProjectorCanAskFor>([
  [
    'ash.projector.creation_flow_enabled',
    PrefsThatProjectorCanAskFor.kProjectorCreationFlowEnabled,
  ],
  [
    'ash.projector.exclude_transcript_dialog_shown',
    PrefsThatProjectorCanAskFor.kProjectorExcludeTranscriptDialogShown,
  ],
]);

const intUserPrefs = new Map<string, PrefsThatProjectorCanAskFor>([
  [
    'ash.projector.gallery_onboarding_show_count',
    PrefsThatProjectorCanAskFor.kProjectorGalleryOnboardingShowCount,
  ],
  [
    'ash.projector.viewer_onboarding_show_count',
    PrefsThatProjectorCanAskFor.kProjectorViewerOnboardingShowCount,
  ],
]);

const requestMaps = new Map<string, RequestType>([
  [
    'POST',
    RequestType.kPost,
  ],
  [
    'GET',
    RequestType.kGet,
  ],
  [
    'PATCH',
    RequestType.kPatch,
  ],
  [
    'DELETE',
    RequestType.kDelete,
  ],
]);

const errorCodeMap = new Map<XhrResponseCode, string>([
  [
    XhrResponseCode.kSuccess,
    '',
  ],
  [
    XhrResponseCode.kTokenFetchFailure,
    'TOKEN_FETCH_FAILURE',
  ],
  [
    XhrResponseCode.kXhrFetchFailure,
    'XHR_FETCH_FAILURE',
  ],
  [
    XhrResponseCode.kUnsupportedURL,
    'UNSUPPORTED_URL',
  ],
  [
    XhrResponseCode.kInvalidAccountEmail,
    'INVALID_ACCOUNT_EMAIL',
  ],
]);

export interface XhrResponseResult {
  success: boolean;
  response?: string;
  error?: string;
  errorCode: JsNetErrorCode;
}

export class UntrustedProjectorBrowserProxyImpl {
  private pageHandlerRemote: UntrustedProjectorPageHandlerRemote;
  private projectorCallbackRouter: UntrustedProjectorPageCallbackRouter;

  constructor() {
    const pageHandlerFactory = UntrustedProjectorPageHandlerFactory.getRemote();
    this.pageHandlerRemote = new UntrustedProjectorPageHandlerRemote();
    this.projectorCallbackRouter = new UntrustedProjectorPageCallbackRouter();
    pageHandlerFactory.create(
        this.pageHandlerRemote.$.bindNewPipeAndPassReceiver(),
        this.projectorCallbackRouter.$.bindNewPipeAndPassRemote());
  }

  getProjectorCallbackRouter(): UntrustedProjectorPageCallbackRouter {
    return this.projectorCallbackRouter;
  }

  async getNewScreencastPreconditionState():
      Promise<NewScreencastPrecondition> {
    const {precondition} =
        await this.pageHandlerRemote.getNewScreencastPrecondition();
    return precondition;
  }

  async shouldDownloadSoda(): Promise<boolean> {
    const {shouldDownload} = await this.pageHandlerRemote.shouldDownloadSoda();
    return shouldDownload;
  }

  async installSoda(): Promise<boolean> {
    const {triggered} = await this.pageHandlerRemote.installSoda();
    return triggered;
  }

  async getPendingScreencasts(): Promise<PendingScreencast[]> {
    const {pendingScreencasts} =
        await this.pageHandlerRemote.getPendingScreencasts();
    return pendingScreencasts;
  }

  async getUserPref(userPref: string): Promise<boolean|number> {
    const isBoolPref = booleanUserPrefs.has(userPref);
    const isIntPref = intUserPrefs.has(userPref);

    if (!(isBoolPref || isIntPref)) {
      throw new Error(`Unsupported user preference: ${userPref}`);
    }

    const mojoPref = isBoolPref ? booleanUserPrefs.get(userPref)! :
                                  intUserPrefs.get(userPref)!;

    const {value} = await this.pageHandlerRemote.getUserPref(mojoPref);

    if (isBoolPref && value.hasOwnProperty('boolValue')) {
      return value.boolValue!;
    }

    if (isIntPref && value.hasOwnProperty('intValue')) {
      return value.intValue!;
    }

    throw new Error(
        `Returned pref value with unexpected type for user preference: ${
            userPref}`);
  }

  async setUserPref(userPref: string, value: boolean|number): Promise<boolean> {
    const isBoolPref = booleanUserPrefs.has(userPref);
    const isIntPref = intUserPrefs.has(userPref);

    if (!(isBoolPref || isIntPref)) {
      throw new Error(`Unsupported user preference: ${userPref}`);
    }

    const mojoPref = isBoolPref ? booleanUserPrefs.get(userPref)! :
                                  intUserPrefs.get(userPref)!;
    const mojoValue = isBoolPref ? {boolValue: value as boolean} :
                                   {intValue: value as number};

    await this.pageHandlerRemote.setUserPref(mojoPref, mojoValue);
    return true;
  }

  async openFeedbackDialog(): Promise<void> {
    await this.pageHandlerRemote.openFeedbackDialog();
    return;
  }

  async startProjectorSession(storageDir: string): Promise<boolean> {
    const {success} = await this.pageHandlerRemote.startProjectorSession({
      path: {
        path: storageDir,
      },
    });
    return success;
  }

  async sendXhr(
      url: string, method: string, requestBody: string|null,
      useCredentials: boolean, useApiKey: boolean,
      headers: {[key: string]: string}|null,
      accountEmail: string|null): Promise<XhrResponseResult> {
    if (!requestMaps.has(method)) {
      throw new Error(`Invalid request method. ${method}`);
    }

    const requestMethod = requestMaps.get(method)!;
    const {response} = await this.pageHandlerRemote.sendXhr(
        url, requestMethod, requestBody, useCredentials, useApiKey, headers,
        accountEmail);

    // TODO(crbug.com/237337607): Remove the success field and just pass
    // response directly.

    const errorCode = response.netErrorCode ?? JsNetErrorCode.kNoError;

    return {
      success: response.responseCode === XhrResponseCode.kSuccess,
      response: response.response,
      error: errorCodeMap.get(response.responseCode),
      errorCode: errorCode,
    };
  }

  async getAccounts(): Promise<Account[]> {
    const {accounts} = await this.pageHandlerRemote.getAccounts();
    return accounts;
  }

  async getVideo(videoFileId: string, resourceKey: string|null):
      Promise<VideoInfo> {
    const {result} =
        await this.pageHandlerRemote.getVideo(videoFileId, resourceKey);
    if ('errorMessage' in result) {
      return Promise.reject(result.errorMessage);
    }
    return result.video!;
  }
}

export const browserProxy = new UntrustedProjectorBrowserProxyImpl();
