// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'onboarding-update-page' is the page that checks to see if the version is up
 * to date before starting the rma process.
 */
export class OnboardingUpdatePageElement extends PolymerElement {
  static get is() {
    return 'onboarding-update-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @protected
       * @type {string}
       */
      currentVersion: {
        type: String,
        value: '',
      },

      /**
       * @protected
       * @type {string}
       */
      currentVersionText_: {
        type: String,
        value: '',
      },

      /**
       * TODO(joonbug): populate this and make private.
       * @type {boolean}
       */
      networkAvailable: {
        type: Boolean,
        value: true,
      },

      /**
       * @protected
       * @type {boolean}
       */
      checkInProgress_: {
        type: Boolean,
        value: false,
      },

      /**
       * @private
       * @type {?ShimlessRmaServiceInterface}
       */
      shimlessRmaService_: {
        type: Object,
        value: null,
      },

      /**
       * @protected
       * @type {boolean}
       */
      updateAvailable_: {
        type: Boolean,
        value: false,
      }
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();
    this.getCurrentVersionText_();
  }

  /**
   * @private
   */
  getCurrentVersionText_() {
    this.shimlessRmaService_.getCurrentOsVersion().then((res) => {
      this.currentVersion = res.version;
      this.currentVersionText_ = `Current version ${this.currentVersion}`;
    });
    // TODO(joonbug): i18n string
  }

  /** @protected */
  onUpdateCheckButtonClicked_() {
    this.checkInProgress_ = true;
    this.shimlessRmaService_.checkForOsUpdates().then((res) => {
      if (res.updateAvailable) {
        this.updateAvailable_ = true;
        // TODO(joonbug): i18n string
        this.currentVersionText_ =
            `Current version ${this.currentVersion} is out of date`;
      } else {
        // TODO(joonbug): i18n string
        this.currentVersionText_ =
            `Current version ${this.currentVersion} is up to date`;
      }
      this.checkInProgress_ = false;
    });
  }

  /** @protected */
  onUpdateButtonClicked_() {
    // TODO(joonbug): trigger update
  }

  /**
   * @protected
   */
  updateCheckButtonHidden_() {
    return !this.networkAvailable || this.updateAvailable_;
  }

  /** @return {!Promise<StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.updateOsSkipped();
  }
};

customElements.define(
    OnboardingUpdatePageElement.is, OnboardingUpdatePageElement);
