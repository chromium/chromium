// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrContainerShadowMixin} from 'chrome://resources/ash/common/cr_elements/cr_container_shadow_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_device_information_page.html.js';
import {FeatureLevel, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {disableNextButton, enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'reimaging-device-information-page' allows the user to update important
 * device information if necessary.
 */

const ReimagingDeviceInformationPageBase =
    I18nMixin(CrContainerShadowMixin(PolymerElement));

/**
 * Supported options for IsChassisBranded and HwComplianceVersion questions.
 */
export enum BooleanOrDefaultOptions {
  DEFAULT = 'default',
  YES = 'yes',
  NO = 'no',
}

export class ReimagingDeviceInformationPage extends
    ReimagingDeviceInformationPageBase {
  static get is() {
    return 'reimaging-device-information-page' as const;
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
       * Set by shimless_rma.ts.
       */
      allButtonsDisabled: Boolean,

      disableResetSerialNumber: {
        type: Boolean,
        computed: 'getDisableResetSerialNumber(originalSerialNumber,' +
            'serialNumber, allButtonsDisabled)',
      },

      disableResetRegion: {
        type: Boolean,
        computed: 'getDisableResetRegion(originalRegionIndex, regionIndex,' +
            'allButtonsDisabled)',
      },

      disableResetSku: {
        type: Boolean,
        computed: 'getDisableResetSku(originalSkuIndex, skuIndex,' +
            'allButtonsDisabled)',
      },

      disableResetCustomLabel: {
        type: Boolean,
        computed: 'getDisableResetCustomLabel(' +
            'originalCustomLabelIndex, customLabelIndex, allButtonsDisabled)',
      },

      disableResetDramPartNumber: {
        type: Boolean,
        computed: 'getDisableResetDramPartNumber(' +
            'originalDramPartNumber, dramPartNumber, allButtonsDisabled)',
      },

      originalSerialNumber: {
        type: String,
        value: '',
      },

      serialNumber: {
        type: String,
        value: '',
      },

      regions: {
        type: Array,
        value: () => [],
      },

      originalRegionIndex: {
        type: Number,
        value: -1,
      },

      regionIndex: {
        type: Number,
        value: -1,
      },

      skus: {
        type: Array,
        value: () => [],
      },

      originalSkuIndex: {
        type: Number,
        value: -1,
      },

      skuIndex: {
        type: Number,
        value: -1,
      },

      customLabels: {
        type: Array,
        value: () => [],
      },

      originalCustomLabelIndex: {
        type: Number,
        value: 0,
      },

      customLabelIndex: {
        type: Number,
        value: 0,
      },

      originalDramPartNumber: {
        type: String,
        value: '',
      },

      dramPartNumber: {
        type: String,
        value: '',
      },

      featureLevel: {
        type: Number,
        value: FeatureLevel.kRmadFeatureLevelUnsupported,
      },

      /**
       * Used to refer to the enum values in the HTML file.
       */
      booleanOrDefaultOptions: {
        type: Object,
        value: BooleanOrDefaultOptions,
        readOnly: true,
      },

      isChassisBranded: {
        type: String,
        value: BooleanOrDefaultOptions.DEFAULT,
      },

      hwComplianceVersion: {
        type: String,
        value: BooleanOrDefaultOptions.DEFAULT,
      },
    };
  }

  allButtonsDisabled: boolean;
  private shimlessRmaService: ShimlessRmaServiceInterface =
      getShimlessRmaService();
  protected isChassisBranded: string;
  protected hwComplianceVersion: string;
  protected booleanOrDefaultOptions: boolean|null;
  protected featureLevel: number;
  protected originalDramPartNumber: string;
  protected dramPartNumber: string;
  protected customLabelIndex: number;
  protected originalCustomLabelIndex: number;
  protected originalRegionIndex: number;
  protected regionIndex: number;
  protected originalSkuIndex: number;
  protected skuIndex: number;
  protected regions: string[];
  protected skus: bigint[]|string[];
  protected customLabels: string[];
  protected disableResetSerialNumber: boolean;
  protected disableResetRegion: boolean;
  protected disableResetSku: boolean;
  protected disableResetCustomLabel: boolean;
  protected disableResetDramPartNumber: boolean;
  protected originalSerialNumber: string;
  protected serialNumber: string;

  override ready() {
    super.ready();
    this.getOriginalSerialNumber();
    this.getOriginalRegionAndRegionList();
    this.getOriginalSkuAndSkuList();
    this.getOriginalCustomLabelAndCustomLabelList();
    this.getOriginalDramPartNumber();
    this.getOriginalFeatureLevel();

    focusPageTitle(this);
  }

  private allInformationIsValid(): boolean {
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

  private updateNextButtonDisabledState(): void {
    const disabled = !this.allInformationIsValid();
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
  }

  private getOriginalSerialNumber(): void {
    this.shimlessRmaService.getOriginalSerialNumber().then((result) => {
      this.originalSerialNumber = result.serialNumber;
      this.serialNumber = this.originalSerialNumber;
    });
  }

  private getOriginalRegionAndRegionList(): void {
    this.shimlessRmaService.getOriginalRegion()
        .then((result: {regionIndex: number}) => {
          this.originalRegionIndex = result.regionIndex;
          return this.shimlessRmaService.getRegionList();
        })
        .then((result: {regions: string[]}) => {
          this.regions = result.regions;
          this.regionIndex = this.originalRegionIndex;

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            const regionSelect: HTMLSelectElement|null =
                this.shadowRoot!.querySelector('#regionSelect');
            assert(regionSelect);
            regionSelect.selectedIndex = this.regionIndex;
          });
        });
  }

  private getOriginalSkuAndSkuList(): void {
    this.shimlessRmaService.getOriginalSku()
        .then((result: {skuIndex: number}) => {
          this.originalSkuIndex = result.skuIndex;
          return this.shimlessRmaService.getSkuList();
        })
        .then((result: {skus: bigint[]}) => {
          this.skus = result.skus;
          this.skuIndex = this.originalSkuIndex;
          return this.shimlessRmaService.getSkuDescriptionList();
        })
        .then((result: {skuDescriptions: string[]}) => {
          // The SKU description list can be empty.
          if (this.skus.length === result.skuDescriptions.length) {
            this.skus = this.skus.map(
                (sku, index) => `${sku}: ${result.skuDescriptions[index]}`);
          }

          // Need to wait for the select options to render before setting the
          // selected index.
          afterNextRender(this, () => {
            const skuSelect: HTMLSelectElement|null =
                this.shadowRoot!.querySelector('#skuSelect');
            assert(skuSelect);
            skuSelect.selectedIndex = this.skuIndex;
          });
        });
  }

  private getOriginalCustomLabelAndCustomLabelList(): void {
    this.shimlessRmaService.getOriginalCustomLabel()
        .then((result: {customLabelIndex: number}) => {
          this.originalCustomLabelIndex = result.customLabelIndex;
          return this.shimlessRmaService.getCustomLabelList();
        })
        .then((result: {customLabels: string[]}) => {
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
            const customLabelSelect: HTMLSelectElement|null =
                this.shadowRoot!.querySelector('#customLabelSelect');
            assert(customLabelSelect);
            customLabelSelect!.selectedIndex = this.customLabelIndex;
          });
        });
  }

  private getOriginalDramPartNumber(): void {
    this.shimlessRmaService.getOriginalDramPartNumber().then(
        (result: {dramPartNumber: string}) => {
          this.originalDramPartNumber = result.dramPartNumber;
          this.dramPartNumber = this.originalDramPartNumber;
        });
  }

  private getOriginalFeatureLevel(): void {
    this.shimlessRmaService.getOriginalFeatureLevel().then(
        (result: {originalFeatureLevel: FeatureLevel}) => {
          this.featureLevel = result.originalFeatureLevel;
        });
  }

  protected getDisableResetSerialNumber(): boolean {
    return this.originalSerialNumber === this.serialNumber ||
        this.allButtonsDisabled;
  }

  protected getDisableResetRegion(): boolean {
    return this.originalRegionIndex === this.regionIndex ||
        this.allButtonsDisabled;
  }

  protected getDisableResetSku(): boolean {
    return this.originalSkuIndex === this.skuIndex || this.allButtonsDisabled;
  }

  protected getDisableResetCustomLabel(): boolean {
    return this.originalCustomLabelIndex === this.customLabelIndex ||
        this.allButtonsDisabled;
  }

  protected getDisableResetDramPartNumber(): boolean {
    return this.originalDramPartNumber === this.dramPartNumber ||
        this.allButtonsDisabled;
  }

  protected onSelectedRegionChange(_e: Event): void {
    const regionSelect: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#regionSelect');
    assert(regionSelect);
    this.regionIndex = regionSelect.selectedIndex;
  }

  protected onSelectedSkuChange(_e: Event): void {
    const skuSelect: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#skuSelect');
    assert(skuSelect);
    this.skuIndex = skuSelect.selectedIndex;
  }

  protected onSelectedCustomLabelChange(_e: Event): void {
    const customLabelSelect: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#customLabelSelect');
    assert(customLabelSelect);
    this.customLabelIndex = customLabelSelect.selectedIndex;
  }

  protected onResetSerialNumberButtonClicked(_e: Event): void {
    this.serialNumber = this.originalSerialNumber;
  }

  protected onResetRegionButtonClicked(_e: Event): void {
    const regionSelect: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#regionSelect');
    assert(regionSelect);
    this.regionIndex = this.originalRegionIndex;
    regionSelect.selectedIndex = this.regionIndex;
  }

  protected onResetSkuButtonClicked(_e: Event): void {
    const skuSelect: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#skuSelect');
    assert(skuSelect);
    this.skuIndex = this.originalSkuIndex;
    skuSelect.selectedIndex = this.skuIndex;
  }

  protected onResetCustomLabelButtonClicked(_e: Event): void {
    const customLabelSelect: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#customLabelSelect');
    assert(customLabelSelect);
    this.customLabelIndex = this.originalCustomLabelIndex;
    customLabelSelect.selectedIndex = this.customLabelIndex;
  }

  protected onResetDramPartNumberButtonClicked(_e: Event): void {
    this.dramPartNumber = this.originalDramPartNumber;
  }

  protected onIsChassisBrandedChange(_e: Event): void {
    const isChassisBranded: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#isChassisBranded');
    assert(isChassisBranded);
    this.isChassisBranded = isChassisBranded.value;
  }

  protected onDoesMeetRequirementsChange(_e: Event): void {
    const doesMeetRequirements: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#doesMeetRequirements');
    assert(doesMeetRequirements);
    this.hwComplianceVersion = doesMeetRequirements.value;
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
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

  private shouldShowComplianceSection(): boolean {
    return this.featureLevel !== FeatureLevel.kRmadFeatureLevelUnsupported;
  }

  private isComplianceStatusKnown(): boolean {
    return this.featureLevel !== FeatureLevel.kRmadFeatureLevelUnsupported &&
        this.featureLevel !== FeatureLevel.kRmadFeatureLevelUnknown;
  }

  private areComplianceQuestionsShown(): boolean {
    return this.shouldShowComplianceSection() &&
        !this.isComplianceStatusKnown();
  }

  private getComplianceStatusString(): string {
    const deviceIsCompliant =
        this.featureLevel >= FeatureLevel.kRmadFeatureLevel1;
    return deviceIsCompliant ? this.i18n('confirmDeviceInfoDeviceCompliant') :
                               this.i18n('confirmDeviceInfoDeviceNotCompliant');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ReimagingDeviceInformationPage.is]: ReimagingDeviceInformationPage;
  }
}

customElements.define(
    ReimagingDeviceInformationPage.is, ReimagingDeviceInformationPage);
