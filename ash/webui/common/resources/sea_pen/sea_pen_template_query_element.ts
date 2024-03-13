// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays template query to search for
 * SeaPen wallpapers.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_icons.html.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_options_element.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getSeaPenTemplates, SeaPenOption, SeaPenTemplate} from './constants.js';
import {SeaPenQuery, SeaPenThumbnail, SeaPenUserVisibleQuery} from './sea_pen.mojom-webui.js';
import {searchSeaPenThumbnails} from './sea_pen_controller.js';
import {SeaPenTemplateChip, SeaPenTemplateId, SeaPenTemplateOption} from './sea_pen_generated.mojom-webui.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {SeaPenPaths} from './sea_pen_router_element.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {getTemplate} from './sea_pen_template_query_element.html.js';
import {ChipToken, getDefaultOptions, getTemplateTokens, isNonEmptyArray, logGenerateSeaPenWallpaper, TemplateToken} from './sea_pen_utils.js';

export class SeaPenTemplateQueryElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-template-query';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      templateId: String,

      path: String,

      seaPenTemplate_: {
        type: Object,
        computed: 'computeSeaPenTemplate_(templateId)',
        observer: 'onSeaPenTemplateChanged_',
      },

      // A map of chip to its selected option. By default, populated after
      // `seaPenTemplate_` is constructed. Updated when the user selects the
      // option on the UI.
      selectedOptions_: {
        type: Object,
        observer: 'onSelectedOptionsChanged_',
      },

      // The tokens generated from `seaPenTemplate_` and `selectedOptions_`.
      templateTokens_: {
        type: Array,
      },

      // The selected chip token. Updated whenever the user clicks a chip in the
      // UI.
      selectedChip_: {
        type: Object,
      },

      // `options_` is an array of possible values for the selected chip. Each
      // "option" will be mapped to a clickable button that the user could
      // select. The options are dependent on the `selectedChip_`.
      options_: {
        type: Array,
      },

      // A boolean indicates whether the user is still selecting chip options.
      isSelectingOptions: {
        type: Boolean,
        reflectToAttribute: true,
      },

      thumbnails_: Object,

      thumbnailsLoading_: Boolean,

      searchButtonText_: {
        type: String,
        value() {
          return loadTimeData.getString('seaPenCreateButton');
        },
      },

      searchButtonIcon_: {
        type: String,
        value() {
          return 'sea-pen:photo-spark';
        },
      },
    };
  }

  path: string;
  // TODO(b/319719709) this should be SeaPenTemplateId.
  templateId: string|null;
  private seaPenTemplate_: SeaPenTemplate;
  private selectedOptions_: Map<SeaPenTemplateChip, SeaPenOption>;
  private templateTokens_: TemplateToken[];
  private options_: SeaPenOption[]|null;
  private selectedChip_: ChipToken|null;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;
  private searchButtonText_: string;
  private searchButtonIcon_: string;
  private isSelectingOptions: boolean;

  static get observers() {
    return ['updateSearchButton_(path, thumbnails_)'];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenTemplateQueryElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
    this.watch<SeaPenTemplateQueryElement['thumbnailsLoading_']>(
        'thumbnailsLoading_', state => state.loading.thumbnails);
    this.updateFromStore();
  }

  private computeSeaPenTemplate_(templateId: string|null) {
    const seaPenTemplates = getSeaPenTemplates();
    const correctTemplate = seaPenTemplates.find(
        (seaPenTemplate) => seaPenTemplate.id.toString() === templateId);
    return correctTemplate as SeaPenTemplate;
  }

  private isChip_(token: any): token is ChipToken {
    return typeof token?.translation === 'string';
  }

  private clearSelectedChipState() {
    this.selectedChip_ = null;
    this.options_ = null;
    this.isSelectingOptions = false;
  }

  private onClickChip_(event: Event&{model: {token: ChipToken}}) {
    assert(this.isChip_(event.model.token), 'Token must be a chip');
    if (this.selectedChip_?.id === event.model.token.id) {
      this.clearSelectedChipState();
    } else {
      this.selectedChip_ = event.model.token;
      assert(
          this.seaPenTemplate_.options.has(this.selectedChip_.id),
          'options must exist');
      this.options_ = this.seaPenTemplate_.options.get(this.selectedChip_.id)!;
      this.isSelectingOptions = true;
    }
  }

  private onClickInspire_() {
    this.selectedOptions_ =
        getDefaultOptions(this.seaPenTemplate_, /*random=*/ true);
    this.templateTokens_ =
        getTemplateTokens(this.seaPenTemplate_, this.selectedOptions_);
    this.onClickSearchButton_();
  }

  private onSeaPenTemplateChanged_(template: SeaPenTemplate) {
    const selectedOptions = getDefaultOptions(template);
    this.clearSelectedChipState();
    this.selectedOptions_ = selectedOptions;
    this.templateTokens_ =
        getTemplateTokens(this.seaPenTemplate_, this.selectedOptions_);
  }

  private onSelectedOptionsChanged_() {
    this.searchButtonText_ = this.i18n('seaPenCreateButton');
    this.searchButtonIcon_ = 'sea-pen:photo-spark';
    this.templateTokens_ =
        getTemplateTokens(this.seaPenTemplate_, this.selectedOptions_);
  }

  private getChipClassName_(chip: ChipToken, selectedChip: ChipToken|null):
      'selected'|'unselected' {
    assert(this.isChip_(chip), 'Token must be a chip');
    // If there are no selected chips, then use the 'selected' styling on all
    // chips.
    return !selectedChip || chip.id === selectedChip.id ? 'selected' :
                                                          'unselected';
  }

  private isOptionSelected_(
      option: SeaPenOption, selectedChipTranslation: string): string {
    return (option.translation === selectedChipTranslation).toString();
  }

  private getOptionClass_(
      option: SeaPenOption, selectedChipTranslation: string): string {
    return this.isOptionSelected_(option, selectedChipTranslation) === 'true' ?
        'action-button' :
        'unselected-option';
  }

  private getTextClassName_(selectedChip: TemplateToken|null): string {
    // Use the 'unselected' styling only if a chip has been selected.
    return selectedChip ? 'unselected' : '';
  }

  private getUserVisibleQueryInfo_(): SeaPenUserVisibleQuery {
    const translatedTokens: string[] = this.templateTokens_.map((token) => {
      return this.isChip_(token) ? token.translation : token;
    });
    return {
      text: translatedTokens.join(' '),
      templateTitle: this.seaPenTemplate_.title,
    };
  }

  private getSeaPenTemplateId_(): SeaPenTemplateId {
    return parseInt(this.templateId!, 10);
  }

  private getTemplateRequest_(): SeaPenQuery {
    const optionMap = new Map<SeaPenTemplateChip, SeaPenTemplateOption>();
    this.selectedOptions_.forEach((option, chip) => {
      optionMap.set(chip, option.value);
    });
    const id = this.getSeaPenTemplateId_();
    assert(!isNaN(id));
    return {
      templateQuery: {
        id,
        options: Object.fromEntries(optionMap),
        userVisibleQuery: this.getUserVisibleQueryInfo_(),
      },
    };
  }

  private onClickSearchButton_() {
    this.clearSelectedChipState();
    searchSeaPenThumbnails(
        this.getTemplateRequest_(), getSeaPenProvider(), this.getStore());
    logGenerateSeaPenWallpaper(this.getSeaPenTemplateId_());
  }

  private updateSearchButton_(
      path: string|null, thumbnails: SeaPenThumbnail[]|null) {
    if (!thumbnails) {
      // The thumbnails are not loaded yet.
      this.searchButtonText_ = this.i18n('seaPenCreateButton');
      this.searchButtonIcon_ = 'sea-pen:photo-spark';
      return;
    }

    switch (path) {
      case SeaPenPaths.RESULTS:
        this.searchButtonText_ = this.i18n('seaPenRecreateButton');
        this.searchButtonIcon_ = 'personalization-shared:refresh';
        break;
      case SeaPenPaths.ROOT:
      default:
        this.searchButtonText_ = this.i18n('seaPenCreateButton');
        this.searchButtonIcon_ = 'sea-pen:photo-spark';
        break;
    }
  }

  private shouldShowOptions_(options: SeaPenOption[]|null): boolean {
    return isNonEmptyArray(options);
  }
}

customElements.define(
    SeaPenTemplateQueryElement.is, SeaPenTemplateQueryElement);

declare global {
  interface HTMLElementTagNameMap {
    'sea-pen-template-query': SeaPenTemplateQueryElement;
  }
}
