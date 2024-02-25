// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AddSupervisionHandler, OAuthTokenFetchStatus} from './add_supervision.mojom-webui.js';
import {AddSupervisionApiServer} from './add_supervision_api_server.js';
import {getTemplate} from './add_supervision_ui.html.js';

declare global {
  interface HTMLElementEventMap {
    'newwindow': chrome.webviewTag.NewWindowEvent;
  }
}

/** List of URL hosts that can be requested by the webview. */
const ALLOWED_HOSTS: string[] = [
  'google.com',
  'gstatic.com',
  'googleapis.com',
  'google-analytics.com',
  // FIFE avatar images (lh3-lh6). See http://go/fife-domains
  'lh3.googleusercontent.com',
  'lh4.googleusercontent.com',
  'lh5.googleusercontent.com',
  'lh6.googleusercontent.com',
];

/**
 * Time in ms to wait before focusing the webview. Refer to the webview's
 * loadstop event listener for details.
 */
const INITIAL_FOCUS_DELAY_MS: number = 50;

/** Returns true if the URL references an HTTP request to localhost. */
export function isLocalHostForTesting(url: URL): boolean {
  return url.protocol === 'http:' && url.hostname === '127.0.0.1';
}

/** Returns true if the URL references one of the allowed hosts. */
function isAllowedHost(url: URL): boolean {
  return url.protocol === 'https:' &&
      ALLOWED_HOSTS.some(
          (allowedHost) =>
              url.host === allowedHost || url.host.endsWith('.' + allowedHost));
}

/** Returns true if the request should be allowed. */
function isAllowedRequest(requestDetails: string): boolean {
  const requestUrl = new URL(requestDetails);
  return isLocalHostForTesting(requestUrl) || isAllowedHost(requestUrl);
}

export interface AddSupervisionUi {
  $: {
    webview: chrome.webviewTag.WebView,
  };
}

export class AddSupervisionUi extends PolymerElement {
  static get is() {
    return 'add-supervision-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      webviewLoading: {
        type: Boolean,
        value: true,
      },
    };
  }

  webviewLoading: boolean;
  private server: AddSupervisionApiServer|null;

  override ready() {
    super.ready();
    const addSupervisionHandler = AddSupervisionHandler.getRemote();
    addSupervisionHandler.getOAuthToken().then((result) => {
      // Setup should terminate early if OAuth Token fetching fails.
      if (result.status === OAuthTokenFetchStatus.ERROR) {
        this.showErrorPage();
        return;
      }

      const webviewUrl = loadTimeData.getString('webviewUrl');
      const eventOriginFilter = loadTimeData.getString('eventOriginFilter');
      const webview = this.$.webview;

      const accessToken = result.oauthToken;
      const flowType = loadTimeData.getString('flowType');
      const platformVersion = loadTimeData.getString('platformVersion');
      const languageCode = loadTimeData.getString('languageCode');

      const url = new URL(webviewUrl);
      url.searchParams.set('flow_type', flowType);
      url.searchParams.set('platform_version', platformVersion);
      url.searchParams.set('access_token', accessToken);
      url.searchParams.set('hl', languageCode);

      // Allow guest webview content to open links in new windows.
      webview.addEventListener(
          'newwindow', (e: chrome.webviewTag.NewWindowEvent) => {
            window.open(e.targetUrl);
          });

      // Change loading indicator on load in order to hide loading spinner.
      webview.addEventListener('contentload', () => {
        this.webviewLoading = false;
      });

      // Sets focus on the inner webview, so that ChromeVox users don't need to
      // navigate through multiple containers when linear navigating through the
      // page (https://crbug.com/1231798).
      // We want the dialog content to be automatically announced once loaded.
      // ChromeVox automatically reads all content when it enters a role=dialog
      // div. If the webview is focused too soon, there's no content to read
      // yet. Therefore we wait for it to be loaded first, with a delay (so that
      // the accessibility tree is fully updated).
      webview.addEventListener('loadstop', () => {
        setTimeout(() => webview.focus(), INITIAL_FOCUS_DELAY_MS);
      });

      webview.addEventListener('loadabort', () => {
        this.webviewLoading = false;
        this.showErrorPage();
      });

      // Block any requests to URLs other than one specified
      // by eventOriginFilter.
      webview.request.onBeforeRequest.addListener((details: {url: string}) => {
        return {cancel: !isAllowedRequest(details.url)};
      }, {urls: ['<all_urls>']}, ['blocking']);

      webview.src = url.toString();

      // Set up the server.
      this.server = new AddSupervisionApiServer(
          this, webview, url.toString(), eventOriginFilter);
    });
  }

  showErrorPage() {
    this.dispatchEvent(new CustomEvent('show-error', {
      bubbles: true,
      composed: true,
    }));
  }

  getApiServerForTest(): AddSupervisionApiServer|null {
    return this.server;
  }
}

customElements.define(AddSupervisionUi.is, AddSupervisionUi);
