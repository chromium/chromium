// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.html.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {IntroBrowserProxy, IntroBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './dice_app.html.js';

export interface IntroAppElement {
  $: {
    acceptSignInButton: CrButtonElement,
    declineSignInButton: CrButtonElement,
    viewManager: CrViewManagerElement,
  };
}

export interface BenefitCard {
  title: string;
  description: string;
  iconName: string;
}

const IntroAppElementBase = I18nMixin(PolymerElement);

export class IntroAppElement extends IntroAppElementBase {
  static get is() {
    return 'intro-app';
  }

  constructor() {
    super();
    this.benefitCards_ = [
      {
        title: this.i18n('devicesCardTitle'),
        description: this.i18n('devicesCardDescription'),
        iconName: 'intro:devices',
      },
      {
        title: this.i18n('securityCardTitle'),
        description: this.i18n('securityCardDescription'),
        iconName: 'cr:security',
      },
      {
        title: this.i18n('backupCardTitle'),
        description: this.i18n('backupCardDescription'),
        iconName: 'intro:cloud-upload',
      },
    ];
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      benefitCards_: {
        type: Array,
      },
    };
  }

  private benefitCards_: BenefitCard[];
  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.setupViewManager_();
  }

  private async setupViewManager_() {
    this.$.viewManager.switchView('splash', 'fade-in', 'fade-out');

    // TODO(crbug.com/1347507): Delay the switch to signInPromo based on the
    // splash animation timing instead.
    await new Promise(resolve => setTimeout(resolve, 1000));

    this.$.viewManager.switchView('signInPromo', 'fade-in', 'no-animation');
  }

  private onContinueWithAccountClick_() {
    this.browserProxy_.continueWithAccount();
  }

  private onContinueWithoutAccountClick_() {
    this.browserProxy_.continueWithoutAccount();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'intro-app': IntroAppElement;
  }
}

customElements.define(IntroAppElement.is, IntroAppElement);
