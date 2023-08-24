// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './parent_access_template.js';
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessEvent} from './parent_access_app.js';
import {ParentAccessController} from './parent_access_controller.js';
import {GetOAuthTokenStatus, ParentAccessServerMessageType} from './parent_access_ui.mojom-webui.js';
import {getParentAccessUIHandler} from './parent_access_ui_handler.js';
import {WebviewManager} from './webview_manager.js';

/**
 * List of URL hosts that can be requested by the webview. The
 * webview URL's host is implicitly included in this list.
 * @const {!Array<string>}
 */
const ALLOWED_HOSTS = [
  'googleapis.com',
  'gstatic.com',
  'googleusercontent.com',
  'google.com',
];

/**
 * The local dev server host, which is the only non-https URL the
 * webview is permitted to load.
 */
const LOCAL_DEV_SERVER_HOST = 'localhost:9879';

class ParentAccessUi extends PolymerElement {
  constructor() {
    super();
    this.webview_manager_ = null;
    this.server = null;
    this.parentAccessUIHandler = getParentAccessUIHandler();
  }

  static get is() {
    return 'parent-access-ui';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      webviewLoading: {type: Boolean, value: true},
    };
  }

  /**
   * Returns whether the provided request should be allowed.
   * @param {string} url Request that is issued by the webview.
   * @return {boolean} Whether the request should be allowed.
   */
  isAllowedRequest(url) {
    const requestUrl = new URL(url);

    // Allow non https only for requests to a local development server webview
    // URL, which would have been specified at the command line.
    if (requestUrl.host === LOCAL_DEV_SERVER_HOST) {
      return true;
    }

    // Otherwise, all requests should be https and in the ALLOWED_HOSTS list.
    const requestIsHttps = requestUrl.protocol === 'https:';
    const requestIsInAllowedHosts = ALLOWED_HOSTS.some(
        (allowedHost) => requestUrl.host == allowedHost ||
            requestUrl.host.endsWith(allowedHost));

    return requestIsHttps && requestIsInAllowedHosts;
  }

  /**
   * Returns whether the provided request should receive auth headers.
   * @param {string} url Request that is issued by the webview.
   * @return {boolean} Whether the request should be allowed.
   */
  shouldReceiveAuthHeader(url) {
    const requestUrl = new URL(url);
    const webviewUrl = new URL(this.webviewUrl_);

    // Only the webviewUrl URL should receive the auth header, because for
    // security reasons, we shouldn't distribute the OAuth token any more
    // broadly that strictly necessary for the widget to function, thereby
    // minimizing the attack surface for the token.
    return requestUrl.host === webviewUrl.host;
  }

  /** @override */
  ready() {
    super.ready();
    this.shadowRoot.querySelector('webview').addEventListener(
        'contentload', () => {
          this.webviewLoading = false;
        });
    this.configureUi().then(
        () => {/* success */},
        (error) => {
          this.showErrorPage_();
        },
    );
  }

  async configureUi() {
    /**
     * @private {string} The initial URL for the webview.
     */
    this.webviewUrl_ =
        (await this.parentAccessUIHandler.getParentAccessURL()).url;

    try {
      const parsedWebviewUrl = new URL(this.webviewUrl_);
      // Set the filter to accept postMessages from the webviewURL's origin
      // only.
      const eventOriginFilter = parsedWebviewUrl.origin;

      const oauthFetchResult = await this.parentAccessUIHandler.getOAuthToken();
      if (oauthFetchResult.status != GetOAuthTokenStatus.kSuccess) {
        throw new Error('OAuth token was not successfully fetched.');
      }

      const webview =
          /** @type {!WebView} */ (this.$.webview);
      const accessToken = oauthFetchResult.oauthToken;

      // Set up the WebviewManager to handle the configuration and
      // access control for the webview.
      this.webview_manager_ = new WebviewManager(webview);
      this.webview_manager_.setAccessToken(accessToken, (url) => {
        return this.shouldReceiveAuthHeader(url);
      });
      this.webview_manager_.setAllowRequestFn((url) => {
        return this.isAllowedRequest(url);
      });

      // Setting the src of the webview triggers the loading process.
      const url = new URL(this.webviewUrl_);
      webview.src = url.toString();

      webview.addEventListener('loadabort', () => {
        this.webviewLoading = false;
        this.showErrorPage_();
      });

      // Set up the controller. It will automatically start the initialization
      // handshake with the hosted content.
      this.server = new ParentAccessController(
          webview, url.toString(), eventOriginFilter);

    } catch (e) {
      this.showErrorPage_();
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

      // Notify ParentAccessUIHandler that we received a ParentAccessCallback.
      // The handler will attempt to parse the callback and return the status.
      const parentAccessServerMessage =
          await this.parentAccessUIHandler.onParentAccessCallbackReceived(
              parentAccessCallback);

      // If the parentAccessCallback couldn't be parsed, then an initialization
      // or communication error occurred between the ParentAccessController and
      // the server.
      if (!(parentAccessServerMessage instanceof Object)) {
        console.error('Error initializing ParentAccessController');
        this.showErrorPage_();
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
          this.showErrorPage_();
          break;
      }
    }
  }

  showErrorPage_() {
    this.dispatchEvent(new CustomEvent(ParentAccessEvent.SHOW_ERROR, {
      bubbles: true,
      composed: true,
    }));
  }
}
customElements.define(ParentAccessUi.is, ParentAccessUi);
