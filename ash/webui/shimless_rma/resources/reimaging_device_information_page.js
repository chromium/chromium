// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_fonts_css.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {FeatureLevel, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton, focusPageTitle, isComplianceCheckEnabled, isSkuDescriptionEnabled} from './shimless_rma_util.js';

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
    mixinBehaviors([I18nBehavior], CrContainerShadowMixin(PolymerElement));

/**
 * Supported options for IsChassisBranded and HwComplianceVersion questions.
 * @enum {string}
 */
export const BooleanOrDefaultOptions = {
  DEFAULT: 'default',
  YES: 'yes',
  NO: 'no',
};

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
      'updateNextButtonDisabledState_(serialNumber_, skuIndex_, regionIndex_,' +
          ' customLabelIndex_, isChassisBranded_, hwComplianceVersion_,' +
          ' featureLevel_)',
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
      disableResetCustomLabel_: {
        type: Boolean,
        computed: 'getDisableResetCustomLabel_(' +
            'originalCustomLabelIndex_, customLabelIndex_, allButtonsDisabled)',
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
      customLabels_: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalCustomLabelIndex_: {
        type: Number,
        value: 0,
      },

      /** @protected */
      customLabelIndex_: {
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

      /** @protected */
      featureLevel_: {
        type: Number,
        value: FeatureLevel.kRmadFeatureLevelUnsupported,
      },

      /**
       * Used to refer to the enum values in the HTML file.
       * @protected {?BooleanOrDefaultOptions}
       */
      booleanOrDefaultOptions_: {
        type: Object,
        value: BooleanOrDefaultOptions,
        readOnly: true,
      },

      /** @protected */
      isChassisBranded_: {
        type: String,
        value: BooleanOrDefaultOptions.DEFAULT,
      },

      /** @protected */
      hwComplianceVersion_: {
        type: String,
        value: BooleanOrDefaultOptions.DEFAULT,
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
    this.getOriginalCustomLabelAndCustomLabelList_();
    this.getOriginalDramPartNumber_();

    if (isComplianceCheckEnabled()) {
      this.getOriginalFeatureLevel_();
    }

    focusPageTitle(this);
  }

  /** @private */
  allInformationIsValid_() {
    const complianceQuestionsHaveDefaultValues =
        this.isChassisBranded_ === BooleanOrDefaultOptions.DEFAULT ||
        this.hwComplianceVersion_ === BooleanOrDefaultOptions.DEFAULT;
    if (this.areComplianceQuestionsShown_() &&
        complianceQuestionsHaveDefaultValues) {
      return false;
    }
    return (this.serialNumber_ !== '') && (this.skuIndex_ >= 0) &&
        (this.regionIndex_ >= 0) && (this.customLabelIndex_ >= 0);
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
          return this.shimlessRmaService_.getSkuDescriptionList();
        })
        .then((result) => {
          // The SKU description list can be empty if the backend disables this
          // feature.
          if (isSkuDescriptionEnabled() &&
              this.skus_.length === result.skuDescriptions.length) {
            this.skus_ = this.skus_.map(
                (sku, index) => `${sku}: ${result.skuDescriptions[index]}`);
          }

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#skuSelect').selectedIndex =
                this.skuIndex_;
          });
        });
  }

  /** @private */
  getOriginalCustomLabelAndCustomLabelList_() {
    this.shimlessRmaService_.getOriginalCustomLabel()
        .then((result) => {
          this.originalCustomLabelIndex_ = result.customLabelIndex;
          return this.shimlessRmaService_.getCustomLabelList();
        })
        .then((result) => {
          this.customLabels_ = result.customLabels;
          const blankIndex = this.customLabels_.indexOf('');
          if (blankIndex >= 0) {
            this.customLabels_[blankIndex] =
                this.i18n('confirmDeviceInfoEmptyCustomLabelLabel');
            if (this.originalCustomLabelIndex_ < 0) {
              this.originalCustomLabelIndex_ = blankIndex;
            }
          }
          this.customLabelIndex_ = this.originalCustomLabelIndex_;

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#customLabelSelect').selectedIndex =
                this.customLabelIndex_;
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

  /** @private */
  getOriginalFeatureLevel_() {
    this.shimlessRmaService_.getOriginalFeatureLevel().then((result) => {
      this.featureLevel_ = result.originalFeatureLevel;
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
  getDisableResetCustomLabel_() {
    return this.originalCustomLabelIndex_ === this.customLabelIndex_ ||
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
  onSelectedCustomLabelChange_(event) {
    this.customLabelIndex_ =
        this.shadowRoot.querySelector('#customLabelSelect').selectedIndex;
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
  onResetCustomLabelButtonClicked_(event) {
    this.customLabelIndex_ = this.originalCustomLabelIndex_;
    this.shadowRoot.querySelector('#customLabelSelect').selectedIndex =
        this.customLabelIndex_;
  }

  /** @protected */
  onResetDramPartNumberButtonClicked_(event) {
    this.dramPartNumber_ = this.originalDramPartNumber_;
  }

  /** @protected */
  onIsChassisBrandedChange_(event) {
    this.isChassisBranded_ =
        this.shadowRoot.querySelector('#isChassisBranded').value;
  }

  /** @protected */
  onDoesMeetRequirementsChange_(event) {
    this.hwComplianceVersion_ =
        this.shadowRoot.querySelector('#doesMeetRequirements').value;
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    if (!this.allInformationIsValid_()) {
      return Promise.reject(new Error('Some required information is not set'));
    } else {
      let isChassisBranded = false;
      let hwComplianceVersion = 0;

      if (this.areComplianceQuestionsShown_()) {
        // Convert isChassisBranded_ to boolean value for mojo.
        isChassisBranded =
            this.isChassisBranded_ === BooleanOrDefaultOptions.YES;

        // Convert hwComplianceVersion_ to correct value for mojo.
        const HARDWARE_COMPLIANT = 1;
        const HARDWARE_NOT_COMPLIANT = 0;
        hwComplianceVersion =
            this.hwComplianceVersion_ === BooleanOrDefaultOptions.YES ?
            HARDWARE_COMPLIANT :
            HARDWARE_NOT_COMPLIANT;
      }

      return this.shimlessRmaService_.setDeviceInformation(
          this.serialNumber_, this.regionIndex_, this.skuIndex_,
          this.customLabelIndex_, this.dramPartNumber_, isChassisBranded,
          hwComplianceVersion);
    }
  }

  /** @private */
  shouldShowComplianceSection_() {
    return isComplianceCheckEnabled() &&
        this.featureLevel_ !== FeatureLevel.kRmadFeatureLevelUnsupported;
  }

  /** @private */
  isComplianceStatusKnown_() {
    return this.featureLevel_ !== FeatureLevel.kRmadFeatureLevelUnsupported &&
        this.featureLevel_ !== FeatureLevel.kRmadFeatureLevelUnknown;
  }

  /** @private */
  areComplianceQuestionsShown_() {
    return this.shouldShowComplianceSection_() &&
        !this.isComplianceStatusKnown_();
  }

  /** @private */
  getComplianceStatusString_() {
    const deviceIsCompliant =
        this.featureLevel_ >= FeatureLevel.kRmadFeatureLevel1;
    return deviceIsCompliant ? this.i18n('confirmDeviceInfoDeviceCompliant') :
                               this.i18n('confirmDeviceInfoDeviceNotCompliant');
  }
}

customElements.define(
    ReimagingDeviceInformationPage.is, ReimagingDeviceInformationPage);
