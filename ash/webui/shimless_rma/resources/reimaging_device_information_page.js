// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_fonts_css.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton, focusPageTitle} from './shimless_rma_util.js';

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

  static get observers() {
    return [
      'updateNextButtonDisabledState_(serialNumber_, skuIndex_, regionIndex_)',
    ];
  }

  static get properties() {
    return {

      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

      /** @protected */
      disableResetSerialNumber_: {
        type: Boolean,
        computed: 'getDisableResetSerialNumber_(originalSerialNumber_,' +
            'serialNumber_, allButtonsDisabled)',
      },

      /** @protected */
      disableResetRegion_: {
        type: Boolean,
        computed: 'getDisableResetRegion_(originalRegionIndex_, regionIndex_,' +
            'allButtonsDisabled)',
      },

      /** @protected */
      disableResetSku_: {
        type: Boolean,
        computed: 'getDisableResetSku_(originalSkuIndex_, skuIndex_,' +
            'allButtonsDisabled)',
      },

      /** @protected */
      disableResetWhiteLabel_: {
        type: Boolean,
        computed: 'getDisableResetWhiteLabel_(' +
            'originalWhiteLabelIndex_, whiteLabelIndex_, allButtonsDisabled)',
      },

      /** @protected */
      disableResetDramPartNumber_: {
        type: Boolean,
        computed: 'getDisableResetDramPartNumber_(' +
            'originalDramPartNumber_, dramPartNumber_, allButtonsDisabled)',
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
        value: -1,
      },

      /** @protected */
      regionIndex_: {
        type: Number,
        value: -1,
      },

      /** @protected {!Array<string>} */
      skus_: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalSkuIndex_: {
        type: Number,
        value: -1,
      },

      /** @protected */
      skuIndex_: {
        type: Number,
        value: -1,
      },

      /** @protected {!Array<string>} */
      whiteLabels_: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalWhiteLabelIndex_: {
        type: Number,
        value: 0,
      },

      /** @protected */
      whiteLabelIndex_: {
        type: Number,
        value: 0,
      },

      /** @protected */
      originalDramPartNumber_: {
        type: String,
        value: '',
      },

      /** @protected */
      dramPartNumber_: {
        type: String,
        value: '',
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
    this.getOriginalWhiteLabelAndWhiteLabelList_();
    this.getOriginalDramPartNumber_();

    focusPageTitle(this);
  }

  /** @private */
  allInformationIsValid_() {
    return (this.serialNumber_ !== '') && (this.skuIndex_ >= 0) &&
        (this.regionIndex_ >= 0);
  }

  /** @private */
  updateNextButtonDisabledState_() {
    const disabled = !this.allInformationIsValid_();
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
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

  /** @private */
  getOriginalWhiteLabelAndWhiteLabelList_() {
    this.shimlessRmaService_.getOriginalWhiteLabel()
        .then((result) => {
          this.originalWhiteLabelIndex_ = result.whiteLabelIndex;
          return this.shimlessRmaService_.getWhiteLabelList();
        })
        .then((result) => {
          this.whiteLabels_ = result.whiteLabels;
          const blankIndex = this.whiteLabels_.indexOf('');
          if (blankIndex >= 0) {
            this.whiteLabels_[blankIndex] =
                this.i18n('confirmDeviceInfoEmptyWhiteLabelLabel');
            if (this.originalWhiteLabelIndex_ < 0) {
              this.originalWhiteLabelIndex_ = blankIndex;
            }
          }
          this.whiteLabelIndex_ = this.originalWhiteLabelIndex_;

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#whiteLabelSelect').selectedIndex =
                this.whiteLabelIndex_;
          });
        });
  }

  /** @private */
  getOriginalDramPartNumber_() {
    this.shimlessRmaService_.getOriginalDramPartNumber().then((result) => {
      this.originalDramPartNumber_ = result.dramPartNumber;
      this.dramPartNumber_ = this.originalDramPartNumber_;
    });
  }

  /** @protected */
  getDisableResetSerialNumber_() {
    return this.originalSerialNumber_ === this.serialNumber_ ||
        this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetRegion_() {
    return this.originalRegionIndex_ === this.regionIndex_ ||
        this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetSku_() {
    return this.originalSkuIndex_ === this.skuIndex_ || this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetWhiteLabel_() {
    return this.originalWhiteLabelIndex_ === this.whiteLabelIndex_ ||
        this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetDramPartNumber_() {
    return this.originalDramPartNumber_ === this.dramPartNumber_ ||
        this.allButtonsDisabled;
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
  onSelectedWhiteLabelChange_(event) {
    this.whiteLabelIndex_ =
        this.shadowRoot.querySelector('#whiteLabelSelect').selectedIndex;
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

  /** @protected */
  onResetWhiteLabelButtonClicked_(event) {
    this.whiteLabelIndex_ = this.originalWhiteLabelIndex_;
    this.shadowRoot.querySelector('#whiteLabelSelect').selectedIndex =
        this.whiteLabelIndex_;
  }

  /** @protected */
  onResetDramPartNumberButtonClicked_(event) {
    this.dramPartNumber_ = this.originalDramPartNumber_;
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    if (!this.allInformationIsValid_()) {
      return Promise.reject(new Error('Some required information is not set'));
    } else {
      return this.shimlessRmaService_.setDeviceInformation(
          this.serialNumber_, this.regionIndex_, this.skuIndex_,
          this.whiteLabelIndex_, this.dramPartNumber_);
    }
  }
}

customElements.define(
    ReimagingDeviceInformationPage.is, ReimagingDeviceInformationPage);
