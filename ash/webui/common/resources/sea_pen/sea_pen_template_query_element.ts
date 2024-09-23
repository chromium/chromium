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
import 'chrome://resources/ash/common/sea_pen/sea_pen.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_chip_text_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_icons.html.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_options_element.js';
import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';

import {LottieRenderer} from 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, beforeNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSeaPenTemplates, SeaPenOption, SeaPenTemplate} from './constants.js';
import {isSeaPenTextInputEnabled, isSeaPenUseExptTemplateEnabled} from './load_time_booleans.js';
import {SeaPenQuery, SeaPenThumbnail, SeaPenUserVisibleQuery} from './sea_pen.mojom-webui.js';
import {getSeaPenThumbnails} from './sea_pen_controller.js';
import {SeaPenTemplateChip, SeaPenTemplateId, SeaPenTemplateOption} from './sea_pen_generated.mojom-webui.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {logGenerateSeaPenWallpaper} from './sea_pen_metrics_logger.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {getTemplate} from './sea_pen_template_query_element.html.js';
import {ChipToken, getDefaultOptions, getSelectedOptionsFromQuery, getTemplateTokens, isNonEmptyArray, isPersonalizationApp, TemplateToken} from './sea_pen_utils.js';
import {getTransitionEnabled} from './transition.js';

// Two options are the same if they have the same key-value pairs.
function isSameOption(
    map1: Map<SeaPenTemplateChip, SeaPenOption>,
    map2: Map<SeaPenTemplateChip, SeaPenOption>): boolean {
  if (map1.size !== map2.size) {
    return false;
  }

  for (const [key, value] of map1.entries()) {
    if (!map2.has(key) || map2.get(key) !== value) {
      return false;
    }
  }

  return true;
}

export interface SeaPenTemplateQueryElement {
  $: {
    container: HTMLDivElement,
  };
}

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

      seaPenQuery_: {
        type: Object,
        value: null,
      },

      seaPenTemplate_: {
        type: Object,
        computed: 'computeSeaPenTemplate_(templateId)',
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

      thumbnails_: {
        type: Object,
        observer: 'updateSearchButton_',
      },

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

      seaPenUseExptTemplateEnabled_: {
        type: Boolean,
        value() {
          return isSeaPenUseExptTemplateEnabled();
        },
      },
    };
  }

  // TODO(b/319719709) this should be SeaPenTemplateId.
  templateId: string|null;
  private seaPenTemplate_: SeaPenTemplate;
  private seaPenQuery_: SeaPenQuery|null;
  private selectedOptions_: Map<SeaPenTemplateChip, SeaPenOption>;
  private templateTokens_: TemplateToken[];
  private options_: SeaPenOption[]|null;
  private selectedChip_: ChipToken|null;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;
  private searchButtonText_: string;
  private searchButtonIcon_: string;
  private isSelectingOptions: boolean;
  private containerOriginalHeight_: number;
  private resizeObserver_: ResizeObserver;
  private seaPenUseExptTemplateEnabled_: boolean;

  static get observers() {
    return [
      'onSeaPenTemplateOrQueryChanged_(seaPenTemplate_, seaPenQuery_)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('click', this.onClick_);
    this.watch<SeaPenTemplateQueryElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
    this.watch<SeaPenTemplateQueryElement['thumbnailsLoading_']>(
        'thumbnailsLoading_', state => state.loading.thumbnails);
    this.watch<SeaPenTemplateQueryElement['seaPenQuery_']>(
        'seaPenQuery_', state => state.currentSeaPenQuery);
    this.updateFromStore();

    this.resizeObserver_ =
        new ResizeObserver(() => this.animateContainerHeight());

    beforeNextRender(this, () => {
      const inspireMeAnimation = this.getInspireMeAnimationElement_();
      if (inspireMeAnimation) {
        inspireMeAnimation.autoplay = false;
      }

      this.containerOriginalHeight_ = this.$.container.scrollHeight;
      this.$.container.style.height = `${this.containerOriginalHeight_}px`;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_.disconnect();
    this.removeEventListener('click', this.onClick_);
  }

  // Called when there is a custom dom-change event dispatched from
  // `sea-pen-options` element.
  private onSeaPenOptionsDomChanged_() {
    const optionsContainer = this.shadowRoot!.querySelector('sea-pen-options');
    if (optionsContainer) {
      this.resizeObserver_.observe(optionsContainer);
    }
  }

  // Updates main container's height and applies transition style.
  private animateContainerHeight() {
    const optionsContainer = this.shadowRoot!.querySelector('sea-pen-options');
    const optionsContainerHeight =
        optionsContainer ? optionsContainer.scrollHeight : 0;
    this.$.container.style.height =
        `${this.containerOriginalHeight_ + optionsContainerHeight}px`;
  }

  // After exiting from the option selection (by "Esc" key or clicking on
  // anywhere), clear the selected chip state and set focus on the last selected
  // chip.
  onOptionSelectionDone() {
    if (!this.selectedChip_) {
      return;
    }
    const selectedChipIndex =
        Array
            .from(this.shadowRoot!.querySelectorAll<HTMLElement>(
                '.chip-container'))
            .findIndex(elem => elem.classList.contains('selected'));
    this.clearSelectedChipState_();
    afterNextRender(this, () => {
      this.shadowRoot!
          .querySelectorAll<HTMLElement>('.chip-text')[selectedChipIndex]
          ?.focus();
    });
  }

  private startInspireIconAnimation_() {
    this.getInspireMeAnimationElement_()?.play();
  }

  private stopInspireIconAnimation_() {
    this.getInspireMeAnimationElement_()?.stop();
  }

  private clearSelectedChipState_() {
    if (this.selectedChip_) {
      this.selectedChip_ = null;
      this.options_ = null;
      this.isSelectingOptions = false;
    }
  }

  private onClick_(): void {
    this.onOptionSelectionDone();
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

  private onClickChip_(event: Event&{model: {token: ChipToken}}) {
    assert(this.isChip_(event.model.token), 'Token must be a chip');
    if (this.selectedChip_?.id === event.model.token.id) {
      this.clearSelectedChipState_();
    } else {
      this.selectedChip_ = event.model.token;
      assert(
          this.seaPenTemplate_.options.has(this.selectedChip_.id),
          'options must exist');
      this.options_ = this.seaPenTemplate_.options.get(this.selectedChip_.id)!;
      this.isSelectingOptions = true;
    }
    // Stop the event propagation, otherwise, the event will be passed to parent
    // element, this.onClick_ will be triggered improperly.
    event.preventDefault();
    event.stopPropagation();
  }

  private onClickInspire_(event: Event) {
    // Run getDefaultOptions (5 times at most) until we get an options that is
    // different from current; which highly likely to happen the first time.
    for (let i = 0; i < 5; i++) {
      const newOptions =
          getDefaultOptions(this.seaPenTemplate_, /*random=*/ true);

      if (!isSameOption(newOptions, this.selectedOptions_)) {
        this.selectedOptions_ = newOptions;
        break;
      }
    }

    this.onClickSearchButton_(event);
  }

  private onSeaPenTemplateOrQueryChanged_(
      template: SeaPenTemplate, seaPenQuery: SeaPenQuery|null) {
    this.clearSelectedChipState_();
    this.selectedOptions_ =
        getSelectedOptionsFromQuery(seaPenQuery, template) ??
        getDefaultOptions(template);
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

  private getInspireMeAnimationElement_(): LottieRenderer|null|undefined {
    return this.shadowRoot?.querySelector<LottieRenderer>(
        '#inspireMeAnimation');
  }

  private onClickSearchButton_(event: Event) {
    this.clearSelectedChipState_();
    getSeaPenThumbnails(
        this.getTemplateRequest_(), getSeaPenProvider(), this.getStore());
    logGenerateSeaPenWallpaper(this.getSeaPenTemplateId_());

    // Stop the event propagation, otherwise, the event will be passed to parent
    // element, this.onClick_ will be triggered improperly.
    event.preventDefault();
    event.stopPropagation();
  }

  private updateSearchButton_(thumbnails: SeaPenThumbnail[]|null) {
    if (!thumbnails) {
      // The thumbnails are not loaded yet.
      this.searchButtonText_ = this.i18n('seaPenCreateButton');
      this.searchButtonIcon_ = 'sea-pen:photo-spark';
    } else {
      this.searchButtonText_ = this.i18n('seaPenRecreateButton');
      this.searchButtonIcon_ = 'personalization-shared:refresh';
    }
  }

  private shouldShowOptions_(options: SeaPenOption[]|null): boolean {
    return isNonEmptyArray(options);
  }

  private shouldShowFreeformNavigationInfo_(): boolean {
    return isSeaPenTextInputEnabled() && isPersonalizationApp();
  }

  private shouldEnableTextAnimation(
      selectedChip: ChipToken|null, token: ChipToken) {
    // enables text animation if the animation is enabled and the chip is
    // selected.
    return getTransitionEnabled() && !!selectedChip &&
        selectedChip.id === token.id;
  }

  private getTemplateAriaLabel_() {
    return this.getUserVisibleQueryInfo_().text;
  }
}

customElements.define(
    SeaPenTemplateQueryElement.is, SeaPenTemplateQueryElement);

declare global {
  interface HTMLElementTagNameMap {
    'sea-pen-template-query': SeaPenTemplateQueryElement;
  }
}
