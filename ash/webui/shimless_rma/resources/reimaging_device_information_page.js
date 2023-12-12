// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_device_information_page.html.js';
import {FeatureLevel, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
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
    return getTemplate();
  }

  static get observers() {
    return [
      'updateNextButtonDisabledState(serialNumber, skuIndex, regionIndex,' +
          ' customLabelIndex, isChassisBranded, hwComplianceVersion,' +
          ' featureLevel)',
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
      disableResetSerialNumber: {
        type: Boolean,
        computed: 'getDisableResetSerialNumber(originalSerialNumber,' +
            'serialNumber, allButtonsDisabled)',
      },

      /** @protected */
      disableResetRegion: {
        type: Boolean,
        computed: 'getDisableResetRegion(originalRegionIndex, regionIndex,' +
            'allButtonsDisabled)',
      },

      /** @protected */
      disableResetSku: {
        type: Boolean,
        computed: 'getDisableResetSku(originalSkuIndex, skuIndex,' +
            'allButtonsDisabled)',
      },

      /** @protected */
      disableResetCustomLabel: {
        type: Boolean,
        computed: 'getDisableResetCustomLabel(' +
            'originalCustomLabelIndex, customLabelIndex, allButtonsDisabled)',
      },

      /** @protected */
      disableResetDramPartNumber: {
        type: Boolean,
        computed: 'getDisableResetDramPartNumber(' +
            'originalDramPartNumber, dramPartNumber, allButtonsDisabled)',
      },

      /** @protected */
      originalSerialNumber: {
        type: String,
        value: '',
      },

      /** @protected */
      serialNumber: {
        type: String,
        value: '',
      },

      /** @protected {!Array<string>} */
      regions: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalRegionIndex: {
        type: Number,
        value: -1,
      },

      /** @protected */
      regionIndex: {
        type: Number,
        value: -1,
      },

      /** @protected {!Array<string>} */
      skus: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalSkuIndex: {
        type: Number,
        value: -1,
      },

      /** @protected */
      skuIndex: {
        type: Number,
        value: -1,
      },

      /** @protected {!Array<string>} */
      customLabels: {
        type: Array,
        value: () => [],
      },

      /** @protected */
      originalCustomLabelIndex: {
        type: Number,
        value: 0,
      },

      /** @protected */
      customLabelIndex: {
        type: Number,
        value: 0,
      },

      /** @protected */
      originalDramPartNumber: {
        type: String,
        value: '',
      },

      /** @protected */
      dramPartNumber: {
        type: String,
        value: '',
      },

      /** @protected */
      featureLevel: {
        type: Number,
        value: FeatureLevel.kRmadFeatureLevelUnsupported,
      },

      /**
       * Used to refer to the enum values in the HTML file.
       * @protected {?BooleanOrDefaultOptions}
       */
      booleanOrDefaultOptions: {
        type: Object,
        value: BooleanOrDefaultOptions,
        readOnly: true,
      },

      /** @protected */
      isChassisBranded: {
        type: String,
        value: BooleanOrDefaultOptions.DEFAULT,
      },

      /** @protected */
      hwComplianceVersion: {
        type: String,
        value: BooleanOrDefaultOptions.DEFAULT,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();
    this.getOriginalSerialNumber();
    this.getOriginalRegionAndRegionList();
    this.getOriginalSkuAndSkuList();
    this.getOriginalCustomLabelAndCustomLabelList();
    this.getOriginalDramPartNumber();

    if (isComplianceCheckEnabled()) {
      this.getOriginalFeatureLevel();
    }

    focusPageTitle(this);
  }

  /** @private */
  allInformationIsValid() {
    const complianceQuestionsHaveDefaultValues =
        this.isChassisBranded === BooleanOrDefaultOptions.DEFAULT ||
        this.hwComplianceVersion === BooleanOrDefaultOptions.DEFAULT;
    if (this.areComplianceQuestionsShown() &&
        complianceQuestionsHaveDefaultValues) {
      return false;
    }
    return (this.serialNumber !== '') && (this.skuIndex >= 0) &&
        (this.regionIndex >= 0) && (this.customLabelIndex >= 0);
  }

  /** @private */
  updateNextButtonDisabledState() {
    const disabled = !this.allInformationIsValid();
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
  }

  /** @private */
  getOriginalSerialNumber() {
    this.shimlessRmaService.getOriginalSerialNumber().then((result) => {
      this.originalSerialNumber = result.serialNumber;
      this.serialNumber = this.originalSerialNumber;
    });
  }

  /** @private */
  getOriginalRegionAndRegionList() {
    this.shimlessRmaService.getOriginalRegion()
        .then((result) => {
          this.originalRegionIndex = result.regionIndex;
          return this.shimlessRmaService.getRegionList();
        })
        .then((result) => {
          this.regions = result.regions;
          this.regionIndex = this.originalRegionIndex;

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#regionSelect').selectedIndex =
                this.regionIndex;
          });
        });
  }

  /** @private */
  getOriginalSkuAndSkuList() {
    this.shimlessRmaService.getOriginalSku()
        .then((result) => {
          this.originalSkuIndex = result.skuIndex;
          return this.shimlessRmaService.getSkuList();
        })
        .then((result) => {
          this.skus = result.skus;
          this.skuIndex = this.originalSkuIndex;
          return this.shimlessRmaService.getSkuDescriptionList();
        })
        .then((result) => {
          // The SKU description list can be empty if the backend disables this
          // feature.
          if (isSkuDescriptionEnabled() &&
              this.skus.length === result.skuDescriptions.length) {
            this.skus = this.skus.map(
                (sku, index) => `${sku}: ${result.skuDescriptions[index]}`);
          }

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#skuSelect').selectedIndex =
                this.skuIndex;
          });
        });
  }

  /** @private */
  getOriginalCustomLabelAndCustomLabelList() {
    this.shimlessRmaService.getOriginalCustomLabel()
        .then((result) => {
          this.originalCustomLabelIndex = result.customLabelIndex;
          return this.shimlessRmaService.getCustomLabelList();
        })
        .then((result) => {
          this.customLabels = result.customLabels;
          const blankIndex = this.customLabels.indexOf('');
          if (blankIndex >= 0) {
            this.customLabels[blankIndex] =
                this.i18n('confirmDeviceInfoEmptyCustomLabelLabel');
            if (this.originalCustomLabelIndex < 0) {
              this.originalCustomLabelIndex = blankIndex;
            }
          }
          this.customLabelIndex = this.originalCustomLabelIndex;

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            this.shadowRoot.querySelector('#customLabelSelect').selectedIndex =
                this.customLabelIndex;
          });
        });
  }

  /** @private */
  getOriginalDramPartNumber() {
    this.shimlessRmaService.getOriginalDramPartNumber().then((result) => {
      this.originalDramPartNumber = result.dramPartNumber;
      this.dramPartNumber = this.originalDramPartNumber;
    });
  }

  /** @private */
  getOriginalFeatureLevel() {
    this.shimlessRmaService.getOriginalFeatureLevel().then((result) => {
      this.featureLevel = result.originalFeatureLevel;
    });
  }

  /** @protected */
  getDisableResetSerialNumber() {
    return this.originalSerialNumber === this.serialNumber ||
        this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetRegion() {
    return this.originalRegionIndex === this.regionIndex ||
        this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetSku() {
    return this.originalSkuIndex === this.skuIndex || this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetCustomLabel() {
    return this.originalCustomLabelIndex === this.customLabelIndex ||
        this.allButtonsDisabled;
  }

  /** @protected */
  getDisableResetDramPartNumber() {
    return this.originalDramPartNumber === this.dramPartNumber ||
        this.allButtonsDisabled;
  }

  /** @protected */
  onSelectedRegionChange(event) {
    this.regionIndex =
        this.shadowRoot.querySelector('#regionSelect').selectedIndex;
  }

  /** @protected */
  onSelectedSkuChange(event) {
    this.skuIndex = this.shadowRoot.querySelector('#skuSelect').selectedIndex;
  }

  /** @protected */
  onSelectedCustomLabelChange(event) {
    this.customLabelIndex =
        this.shadowRoot.querySelector('#customLabelSelect').selectedIndex;
  }

  /** @protected */
  onResetSerialNumberButtonClicked(event) {
    this.serialNumber = this.originalSerialNumber;
  }

  /** @protected */
  onResetRegionButtonClicked(event) {
    this.regionIndex = this.originalRegionIndex;
    this.shadowRoot.querySelector('#regionSelect').selectedIndex =
        this.regionIndex;
  }

  /** @protected */
  onResetSkuButtonClicked(event) {
    this.skuIndex = this.originalSkuIndex;
    this.shadowRoot.querySelector('#skuSelect').selectedIndex = this.skuIndex;
  }

  /** @protected */
  onResetCustomLabelButtonClicked(event) {
    this.customLabelIndex = this.originalCustomLabelIndex;
    this.shadowRoot.querySelector('#customLabelSelect').selectedIndex =
        this.customLabelIndex;
  }

  /** @protected */
  onResetDramPartNumberButtonClicked(event) {
    this.dramPartNumber = this.originalDramPartNumber;
  }

  /** @protected */
  onIsChassisBrandedChange(event) {
    this.isChassisBranded =
        this.shadowRoot.querySelector('#isChassisBranded').value;
  }

  /** @protected */
  onDoesMeetRequirementsChange(event) {
    this.hwComplianceVersion =
        this.shadowRoot.querySelector('#doesMeetRequirements').value;
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    if (!this.allInformationIsValid()) {
      return Promise.reject(new Error('Some required information is not set'));
    } else {
      let isChassisBranded = false;
      let hwComplianceVersion = 0;

      if (this.areComplianceQuestionsShown()) {
        // Convert isChassisBranded to boolean value for mojo.
        isChassisBranded =
            this.isChassisBranded === BooleanOrDefaultOptions.YES;

        // Convert hwComplianceVersion_ to correct value for mojo.
        const HARDWARE_COMPLIANT = 1;
        const HARDWARE_NOT_COMPLIANT = 0;
        hwComplianceVersion =
            this.hwComplianceVersion === BooleanOrDefaultOptions.YES ?
            HARDWARE_COMPLIANT :
            HARDWARE_NOT_COMPLIANT;
      }

      return this.shimlessRmaService.setDeviceInformation(
          this.serialNumber, this.regionIndex, this.skuIndex,
          this.customLabelIndex, this.dramPartNumber, isChassisBranded,
          hwComplianceVersion);
    }
  }

  /** @private */
  shouldShowComplianceSection() {
    return isComplianceCheckEnabled() &&
        this.featureLevel !== FeatureLevel.kRmadFeatureLevelUnsupported;
  }

  /** @private */
  isComplianceStatusKnown() {
    return this.featureLevel !== FeatureLevel.kRmadFeatureLevelUnsupported &&
        this.featureLevel !== FeatureLevel.kRmadFeatureLevelUnknown;
  }

  /** @private */
  areComplianceQuestionsShown() {
    return this.shouldShowComplianceSection() &&
        !this.isComplianceStatusKnown();
  }

  /** @private */
  getComplianceStatusString() {
    const deviceIsCompliant =
        this.featureLevel >= FeatureLevel.kRmadFeatureLevel1;
    return deviceIsCompliant ? this.i18n('confirmDeviceInfoDeviceCompliant') :
                               this.i18n('confirmDeviceInfoDeviceNotCompliant');
  }
}

customElements.define(
    ReimagingDeviceInformationPage.is, ReimagingDeviceInformationPage);
