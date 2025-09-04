// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../components/buttons/oobe_text_button.js';

import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {FjordStationSetupPageHandlerRemote} from '../../mojom-webui/screens_common.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './fjord_station_setup.html.js';

export const FjordStationSetupScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

const ALLOWED_ORIGINS = [
  'http://codec.localhost:27702',  // The web server where the UI is located.
  'https://fonts.googleapis.com',  // For fonts and icons.
  'https://fonts.gstatic.com',     // For fonts and icons.
];

const FINISH_SETUP_URL = 'http://codec.localhost:27702/oobe/finish-setup';

enum StationSetupPage {
  STATION_SETUP = 'StationSetup',
  FINISH_SETUP = 'FinishSetup',
}

export class FjordStationSetupScreen extends
    FjordStationSetupScreenElementBase {
  static get is() {
    return 'fjord-station-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  private webview: chrome.webviewTag.WebView;
  private currentPage: StationSetupPage;
  private handler: FjordStationSetupPageHandlerRemote;

  override ready(): void {
    super.ready();
    this.handler = new FjordStationSetupPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory.establishFjordStationSetupScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver());

    this.webview =
        this.shadowRoot!.querySelector<chrome.webviewTag.WebView>('webview')!;
    this.currentPage = StationSetupPage.STATION_SETUP;

    // Intercept all requests and block them if they are not allowed origins.
    this.webview.request.onBeforeRequest.addListener((details) => {
      return {cancel: !this.urlMatchesAllowedOrigin(details.url)};
    }, {urls: ['<all_urls>']}, ['blocking']);

    this.initializeLoginScreen('FjordStationSetupScreen');
  }

  private onDoneButtonClicked(): void {
    // Switch to the finish setup page URL after station setup is complete.
    // Call the onSetupComplete API when the user is done with the finish setup
    // page.
    if (this.currentPage === StationSetupPage.STATION_SETUP) {
      this.webview.src = FINISH_SETUP_URL;
      this.shadowRoot!.querySelector('#primaryButton')!.setAttribute(
          'text-key', 'fjordStationSetupDoneButton');
      this.currentPage = StationSetupPage.FINISH_SETUP;
    } else {
      this.handler.onSetupComplete();
    }
  }

  private urlMatchesAllowedOrigin(url: string): boolean {
    const requestUrl = new URL(url);
    if (ALLOWED_ORIGINS.includes(requestUrl.origin)) {
      return true;
    }
    return false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FjordStationSetupScreen.is]: FjordStationSetupScreen;
  }
}

customElements.define(FjordStationSetupScreen.is, FjordStationSetupScreen);
