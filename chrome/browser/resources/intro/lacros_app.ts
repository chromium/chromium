// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/tangible_sync_style_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {IntroBrowserProxy, LacrosIntroProfileInfo} from './browser_proxy.js';
import {IntroBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './lacros_app.html.js';

const LacrosIntroAppElementBase = WebUiListenerMixin(PolymerElement);

// Exported for testing
export interface LacrosIntroAppElement {
  $: {
    proceedButton: CrButtonElement,
  };
}

export class LacrosIntroAppElement extends LacrosIntroAppElementBase {
  static get is() {
    return 'intro-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** URL for the profile picture */
      pictureUrl_: String,

      /** The title of the screen */
      title_: String,

      /** The subtitle of the screen */
      subtitle_: String,

      /** The detailed info about enterprise management */
      managementDisclaimer_: String,

      disableProceedButton_: {
        type: Boolean,
        value: false,
      },
    };
  }

  override ready() {
    super.ready();

    this.addWebUiListener(
        'on-profile-info-changed', this.setProfileInfo_.bind(this));
    this.browserProxy_.initializeMainView();
  }

  private pictureUrl_: string;
  private title_: string;
  private subtitle_: string;
  private managementDisclaimer_: string;
  private disableProceedButton_: boolean;
  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();


  private setProfileInfo_(info: LacrosIntroProfileInfo) {
    this.pictureUrl_ = info.pictureUrl;
    this.title_ = info.title;
    this.subtitle_ = info.subtitle;
    this.managementDisclaimer_ = info.managementDisclaimer;
  }

  /** Called when the proceed button is clicked. */
  private onProceed_() {
    this.disableProceedButton_ = true;
    this.browserProxy_.continueWithAccount();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'intro-app': LacrosIntroAppElement;
  }
}

customElements.define(LacrosIntroAppElement.is, LacrosIntroAppElement);
