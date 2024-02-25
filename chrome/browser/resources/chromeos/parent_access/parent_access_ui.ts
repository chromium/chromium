// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './parent_access_template.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessEvent} from './parent_access_app.js';
import {ParentAccessController} from './parent_access_controller.js';
import {getTemplate} from './parent_access_ui.html.js';
import {GetOauthTokenStatus, ParentAccessServerMessageType, ParentAccessUiHandlerInterface} from './parent_access_ui.mojom-webui.js';
import {getParentAccessUiHandler} from './parent_access_ui_handler.js';
import {WebviewManager} from './webview_manager.js';

export interface ParentAccessUi {
  $: {
    webview: chrome.webviewTag.WebView,
  };
}

/**
 * List of URL hosts that can be requested by the webview. The
 * webview URL's host is implicitly included in this list.
 */
const ALLOWED_HOSTS: string[] = [
  'googleapis.com',
  'gstatic.com',
  'googleusercontent.com',
  'google.com',
];

/**
 * The local dev server host, which is the only non-https URL the
 * webview is permitted to load.
 */
const LOCAL_DEV_SERVER_HOST: string = 'localhost:9879';

export class ParentAccessUi extends PolymerElement {
  static get is() {
    return 'parent-access-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      webviewLoading: {type: Boolean, value: true},
    };
  }

  webviewLoading: boolean;
  private webviewManager: WebviewManager;
  private server: ParentAccessController;
  private parentAccessUiHandler: ParentAccessUiHandlerInterface;
  private webviewUrl: string;

  constructor() {
    super();
    this.parentAccessUiHandler = getParentAccessUiHandler();
  }

  override ready() {
    super.ready();
    this.shadowRoot!.querySelector('webview')!.addEventListener(
        'contentload', () => {
          this.webviewLoading = false;
        });
    this.configureUi().then(
        () => {/* success */},
        () => {
          this.showErrorPage();
        },
    );
  }

  isAllowedRequest(url: string): boolean {
    const requestUrl = new URL(url);

    // Allow non https only for requests to a local development server webview
    // URL, which would have been specified at the command line.
    if (requestUrl.host === LOCAL_DEV_SERVER_HOST) {
      return true;
    }

    // Otherwise, all requests should be https and in the ALLOWED_HOSTS list.
    const requestIsHttps = requestUrl.protocol === 'https:';
    const requestIsInAllowedHosts = ALLOWED_HOSTS.some(
        (allowedHost) => requestUrl.host === allowedHost ||
            requestUrl.host.endsWith(allowedHost));

    return requestIsHttps && requestIsInAllowedHosts;
  }

  shouldReceiveAuthHeader(url: string): boolean {
    const requestUrl = new URL(url);
    const webviewUrl = new URL(this.webviewUrl);

    // Only the webviewUrl URL should receive the auth header, because for
    // security reasons, we shouldn't distribute the OAuth token any more
    // broadly that strictly necessary for the widget to function, thereby
    // minimizing the attack surface for the token.
    return requestUrl.host === webviewUrl.host;
  }

  async configureUi() {
    this.webviewUrl =
        (await this.parentAccessUiHandler.getParentAccessUrl()).url;

    try {
      const parsedWebviewUrl = new URL(this.webviewUrl);
      // Set the filter to accept postMessages from the webviewURL's origin
      // only.
      const eventOriginFilter = parsedWebviewUrl.origin;

      const oauthFetchResult = await this.parentAccessUiHandler.getOauthToken();
      if (oauthFetchResult.status !== GetOauthTokenStatus.kSuccess) {
        throw new Error('OAuth token was not successfully fetched.');
      }

      const webview = this.$.webview;
      const accessToken = oauthFetchResult.oauthToken;

      // Set up the WebviewManager to handle the configuration and
      // access control for the webview.
      this.webviewManager = new WebviewManager(webview);
      this.webviewManager.setAccessToken(accessToken, (url: string) => {
        return this.shouldReceiveAuthHeader(url);
      });
      this.webviewManager.setAllowRequestFn((url: string) => {
        return this.isAllowedRequest(url);
      });

      // Setting the src of the webview triggers the loading process.
      const url = new URL(this.webviewUrl);
      webview.src = url.toString();

      webview.addEventListener('loadabort', () => {
        this.webviewLoading = false;
        this.showErrorPage();
      });

      // Set up the controller. It will automatically start the initialization
      // handshake with the hosted content.
      this.server = new ParentAccessController(
          webview, url.toString(), eventOriginFilter);
    } catch (e) {
      this.showErrorPage();
      return;
    }


    // What follows is the main message handling loop.  The received base64
    // encoded proto messages are passed to c++ handler for proto decoding
    // before they are handled. When the following while loop terminates, the
    // flow will either proceed to the next steps, or show a terminal error.
    let lastServerMessageType = ParentAccessServerMessageType.kIgnore;

    while (lastServerMessageType === ParentAccessServerMessageType.kIgnore) {
      const parentAccessCallback = await Promise.race([
        this.server.whenParentAccessCallbackReceived(),
        this.server.whenInitializationError(),
      ]);

      // Notify ParentAccessUiHandler that we received a ParentAccessCallback.
      // The handler will attempt to parse the callback and return the status.
      const parentAccessServerMessage =
          await this.parentAccessUiHandler.onParentAccessCallbackReceived(
              parentAccessCallback);

      // If the parentAccessCallback couldn't be parsed, then an initialization
      // or communication error occurred between the ParentAccessController and
      // the server.
      if (!(parentAccessServerMessage instanceof Object)) {
        console.error('Error initializing ParentAccessController');
        this.showErrorPage();
        break;
      }

      lastServerMessageType = parentAccessServerMessage.message.type;

      switch (lastServerMessageType) {
        case ParentAccessServerMessageType.kParentVerified:
          this.dispatchEvent(new CustomEvent(ParentAccessEvent.SHOW_AFTER, {
            bubbles: true,
            composed: true,
          }));
          break;

        case ParentAccessServerMessageType.kIgnore:
          continue;

        case ParentAccessServerMessageType.kError:
        default:
          this.showErrorPage();
          break;
      }
    }
  }

  private showErrorPage() {
    this.dispatchEvent(new CustomEvent(ParentAccessEvent.SHOW_ERROR, {
      bubbles: true,
      composed: true,
    }));
  }
}
customElements.define(ParentAccessUi.is, ParentAccessUi);
