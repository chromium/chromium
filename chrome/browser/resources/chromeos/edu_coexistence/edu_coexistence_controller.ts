// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AuthCompletedCredentials, Authenticator, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {PostMessageApiServer} from 'chrome://resources/ash/common/post_message_api/post_message_api_server.js';

import {EduCoexistenceBrowserProxy, EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';

const MILLISECONDS_PER_SECOND = 1000;

export interface EduCoexistenceParams {
  hl: string;
  url: string;
  clientId: string;
  sourceUi: string;
  clientVersion: string;
  eduCoexistenceAccessToken: string;
  eduCoexistenceId: string;
  platformVersion: string;
  releaseChannel: string;
  deviceId: string;
  email?: string;
  readOnlyEmail?: string;
  signinTime: number;
}

function constructEduCoexistenceUrl(params: EduCoexistenceParams): URL {
  const url = new URL(params.url);
  url.searchParams.set('hl', params.hl);
  url.searchParams.set('source_ui', params.sourceUi);
  url.searchParams.set('client_id', params.clientId);
  url.searchParams.set('client_version', params.clientVersion);
  url.searchParams.set('edu_coexistence_id', params.eduCoexistenceId);
  url.searchParams.set('platform_version', params.platformVersion);
  url.searchParams.set('release_channel', params.releaseChannel);
  url.searchParams.set('device_id', params.deviceId);
  if (params.email) {
    url.searchParams.set('email', params.email);
    if (params.readOnlyEmail) {
      url.searchParams.set('read_only_email', params.readOnlyEmail);
    }
  }
  return url;
}

/**
 * Class that orchestrates the EDU Coexistence signin flow.
 */
export class EduCoexistenceController extends PostMessageApiServer {
  authenticator: Authenticator;
  private ui: Element;
  private isOobe: boolean;
  private flowUrl: URL;
  private originUrlPrefix: string;
  private webview: chrome.webviewTag.WebView;
  private authCompletedReceived: boolean;
  private browserProxy: EduCoexistenceBrowserProxy;
  private eduCoexistenceAccessToken: string;
  private signinTime: number;
  private isDomLoaded: boolean;
  private guestFlowState: number|null;
  private userInfo: any;

  constructor(ui: Element, webview: Element, params: EduCoexistenceParams) {
    const flowUrl = constructEduCoexistenceUrl(params);
    const protocol = flowUrl.hostname === 'localhost' ? 'http://' : 'https://';
    const originUrlPrefix = protocol + flowUrl.host;
    super(webview, originUrlPrefix, originUrlPrefix);

    this.ui = ui;
    this.isOobe = params.sourceUi === 'oobe';
    this.flowUrl = flowUrl;
    this.originUrlPrefix = originUrlPrefix;
    this.webview = webview as chrome.webviewTag.WebView;
    this.userInfo = null;
    this.authCompletedReceived = false;
    this.browserProxy = EduCoexistenceBrowserProxyImpl.getInstance();
    this.eduCoexistenceAccessToken = params.eduCoexistenceAccessToken;
    this.signinTime = params.signinTime;

    this.webview.request.onBeforeSendHeaders.addListener(
        (details) => {
          if (this.originMatchesFilter(details.url)) {
            details.requestHeaders.push({
              name: 'Authorization',
              value: 'Bearer ' + this.eduCoexistenceAccessToken,
            });
          }

          return {requestHeaders: details.requestHeaders};
        },

        {urls: ['<all_urls>']}, ['blocking', 'requestHeaders']);

    /**
     * The state of the guest content, saved as requested by
     * the guest content to ensure that its state outlives content
     * reload events, which destroy the state of the guest content.
     * The value itself is opaque encoded binary data.
     */
    this.guestFlowState = null;
    this.authenticator = new Authenticator(this.webview);

    this.isDomLoaded = document.readyState !== 'loading';
    if (this.isDomLoaded) {
      this.initializeAfterDomLoaded();
    } else {
      document.addEventListener(
          'DOMContentLoaded', this.initializeAfterDomLoaded.bind(this));
    }
  }

  override onInitializationError(origin: string) {
    this.reportError(
        ['Error initializing communication channel with origin:' + origin]);
  }

  getIsOobe(): boolean {
    return this.isOobe;
  }

  /**
   * Returns the hostname of the origin of the flow's URL (the one it was
   * initialized with, not its current URL).
   */
  getFlowOriginHostname(): string {
    return this.flowUrl.hostname;
  }

  private initializeAfterDomLoaded() {
    this.isDomLoaded = true;
    // Register methods with PostMessageAPI.
    this.registerMethod('consentValid', this.consentValid.bind(this));
    this.registerMethod('consentLogged', this.consentLogged.bind(this));
    this.registerMethod('requestClose', this.requestClose.bind(this));
    this.registerMethod('reportError', this.reportError.bind(this));
    this.registerMethod(
        'saveGuestFlowState', this.saveGuestFlowState.bind(this));
    this.registerMethod(
        'fetchGuestFlowState', this.fetchGuestFlowState.bind(this));
    this.registerMethod(
        'getEduAccountEmail', this.getEduAccountEmail.bind(this));
    this.registerMethod(
        'getTimeDeltaSinceSigninSeconds',
        this.getTimeDeltaSinceSigninSeconds.bind(this));

    // Add listeners for Authenticator.
    this.addAuthenticatorListeners();
  }

  /**
   * Loads the flow into the controller.
   */
  loadAuthenticator(data: AuthParams) {
    // We use the Authenticator to set the web flow URL instead
    // of setting it ourselves, so that the content isn't loaded twice.
    // This is why this class doesn't directly set webview.src_ (except in
    // onAuthCompleted below to handle the corner case of loading
    // accounts.google.com for running against webserver running on localhost).
    // The EDU Coexistence web flow will be responsible for constructing
    // and forwarding to the accounts.google.com URL that Authenticator
    // interacts with.
    data.frameUrl = this.flowUrl;
    this.authenticator.load(data.authMode, data);
  }

  /**
   * Resets the internal state of the controller.
   */
  reset() {
    this.userInfo = null;
    this.authCompletedReceived = false;
  }

  private addAuthenticatorListeners() {
    this.authenticator.addEventListener('ready', () => this.onAuthReady());
    this.authenticator.addEventListener(
        'getAccounts', () => this.onGetAccounts());
    this.authenticator.addEventListener(
        'getDeviceId', () => this.onGetDeviceId());
    this.authenticator.addEventListener(
        'authCompleted',
        e => this.onAuthCompleted(e as CustomEvent<AuthCompletedCredentials>));
  }

  private onAuthReady() {
    this.browserProxy.authenticatorReady();
  }

  private onGetAccounts() {
    this.browserProxy.getAccounts().then(result => {
      this.authenticator.getAccountsResponse(result);
    });
  }

  private onGetDeviceId() {
    this.browserProxy.getDeviceId().then(deviceId => {
      this.authenticator.getDeviceIdResponse(deviceId);
    });
  }

  private onAuthCompleted(e: CustomEvent<AuthCompletedCredentials>) {
    this.authCompletedReceived = true;
    this.userInfo = e.detail;
    this.browserProxy.completeLogin(e.detail);

    // The EDU Signin page doesn't forward to the next page on success, so we
    // have to manually update the src to continue to the last page of the flow.
    const finishUrl = this.flowUrl;
    finishUrl.pathname = '/supervision/coexistence/finish';
    this.webview.src = finishUrl.toString();
  }

  /** Informs API that the parent consent is now valid. */
  private consentValid() {
    this.browserProxy.consentValid();
  }

  private consentLogged(eduCoexistenceToSVersion: string[]): Promise<boolean> {
    // The first argument of eduCoexistenceToSVersion contains the ToS version.
    return this.browserProxy.consentLogged(
        this.userInfo.email, eduCoexistenceToSVersion[0]);
  }

  /** Attempts to close the widget hosting the flow. */
  private requestClose() {
    this.browserProxy.dialogClose();
  }

  private saveGuestFlowState(guestFlowState: number[]) {
    // The first argument of guestFlowState contains the guest flow state.
    this.guestFlowState = guestFlowState[0];
  }

  /**
   * Returns the guest flow state previously saved using saveGuestFlowState().
   */
  private fetchGuestFlowState(): {'state': number|null} {
    return {'state': this.guestFlowState};
  }

  private getEduAccountEmail(): string {
    console.assert(this.userInfo);
    return this.userInfo.email;
  }

  /**
   * Notifies the API that there was an unrecoverable error during the flow.
   * Takes an array that contains the error message at index 0.
   */
  private reportError(error: string[]) {
    // Notify the app to switch to error screen.
    this.ui.dispatchEvent(new CustomEvent('go-error'));

    // Send the error strings to C++ handler so they are logged.
    this.browserProxy.onError(error);
  }

  /**
   * Made public for testing purposes.
   * Returns the number of seconds that have elapsed since the user's initial
   * signin.
   */
  getTimeDeltaSinceSigninSeconds(): number {
    return (Date.now() - this.signinTime) / MILLISECONDS_PER_SECOND;
  }
}
