// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'reimaging-device-information-page' allows the user to update important
 * device information if necessary.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ReimagingDeviceInformationPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ReimagingDeviceInformationPage extends
    ReimagingDeviceInformationPageBase {
  static get is() {
    return 'reimaging-device-information-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      disableResetSerialNumber_: {
        type: Boolean,
        computed:
            'getDisableResetSerialNumber_(originalSerialNumber_, serialNumber_)',
      },

      /** @protected */
      disableResetRegion_: {
        type: Boolean,
        computed: 'getDisableResetRegion_(originalRegionIndex_, regionIndex_)',
      },

      /** @protected */
      disableResetSku_: {
        type: Boolean,
        computed: 'getDisableResetSku_(originalSkuIndex_, skuIndex_)',
      },

      /** @protected */
      originalSerialNumber_: {
        type: String,
        value: '',
      },

      /** @protected */
      serialNumber_: {
        type: String,
        value: '',
      },

      /** @protected {!Array<string>} */
      regions_: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalRegionIndex_: {
        type: Number,
        value: 0,
      },

      /** @protected */
      regionIndex_: {
        type: Number,
        value: 0,
      },

      /** @protected {!Array<string>} */
      skus_: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalSkuIndex_: {
        type: Number,
        value: 0,
      },

      /** @protected */
      skuIndex_: {
        type: Number,
        value: 0,
      },

    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();
    this.getOriginalSerialNumber_();
    this.getOriginalRegionAndRegionList_();
    this.getOriginalSkuAndSkuList_();
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  /** @private */
  getOriginalSerialNumber_() {
    this.shimlessRmaService_.getOriginalSerialNumber().then((result) => {
      this.originalSerialNumber_ = result.serialNumber;
      this.serialNumber_ = this.originalSerialNumber_;
    });
  }

  /** @private */
  getOriginalRegionAndRegionList_() {
    this.shimlessRmaService_.getOriginalRegion()
        .then((result) => {
          this.originalRegionIndex_ = result.regionIndex;
          return this.shimlessRmaService_.getRegionList();
        })
        .then((result) => {
          this.regions_ = result.regions;
          this.regionIndex_ = this.originalRegionIndex_;

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#regionSelect').selectedIndex =
                this.regionIndex_;
          });
        });
  }

  /** @private */
  getOriginalSkuAndSkuList_() {
    this.shimlessRmaService_.getOriginalSku()
        .then((result) => {
          this.originalSkuIndex_ = result.skuIndex;
          return this.shimlessRmaService_.getSkuList();
        })
        .then((result) => {
          this.skus_ = result.skus;
          this.skuIndex_ = this.originalSkuIndex_;

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#skuSelect').selectedIndex =
                this.skuIndex_;
          });
        });
  }

  /** @protected */
  getDisableResetSerialNumber_() {
    return this.originalSerialNumber_ === this.serialNumber_;
  }

  /** @protected */
  getDisableResetRegion_() {
    return this.originalRegionIndex_ === this.regionIndex_;
  }

  /** @protected */
  getDisableResetSku_() {
    return this.originalSkuIndex_ === this.skuIndex_;
  }

  /** @protected */
  onSelectedRegionChange_(event) {
    this.regionIndex_ =
        this.shadowRoot.querySelector('#regionSelect').selectedIndex;
  }

  /** @protected */
  onSelectedSkuChange_(event) {
    this.skuIndex_ = this.shadowRoot.querySelector('#skuSelect').selectedIndex;
  }

  /** @protected */
  onResetSerialNumberButtonClicked_(event) {
    this.serialNumber_ = this.originalSerialNumber_;
  }

  /** @protected */
  onResetRegionButtonClicked_(event) {
    this.regionIndex_ = this.originalRegionIndex_;
    this.shadowRoot.querySelector('#regionSelect').selectedIndex =
        this.regionIndex_;
  }

  /** @protected */
  onResetSkuButtonClicked_(event) {
    this.skuIndex_ = this.originalSkuIndex_;
    this.shadowRoot.querySelector('#skuSelect').selectedIndex = this.skuIndex_;
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.serialNumber_ === '') {
      return Promise.reject(new Error('Serial number not set'));
    } else {
      return this.shimlessRmaService_.setDeviceInformation(
          this.serialNumber_, this.regionIndex_, this.skuIndex_);
    }
  }
}

customElements.define(
    ReimagingDeviceInformationPage.is, ReimagingDeviceInformationPage);
