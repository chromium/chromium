// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays template query to search for
 * SeaPen wallpapers.
 */

import '../../../common/icons.html.js';

import {assert} from 'chrome://resources/js/assert.js';

import {WithPersonalizationStore} from '../../personalization_store.js';
import {isNonEmptyArray} from '../../utils.js';
import {getSampleSeaPenTemplates, parseTemplateText, SeaPenOption, SeaPenTemplate} from '../utils.js';

import {getTemplate} from './sea_pen_template_query_element.html.js';

/**
 * Returns a random number between [0, max).
 */
function getRandomInt(max: number) {
  return Math.floor(Math.random() * max);
}

function isChip(word: string): boolean {
  return !!word && word.startsWith('<') && word.endsWith('>');
}

/**
 * A template token that is a chip.
 */
export interface ChipToken {
  // The translated string displayed on the UI.
  translation: string;
  // The identifier of the chip .e.g. <city> or <style>.
  id: string;
}

/**
 * A tokenized unit of the `SeaPenTemplate`. Used to render the prompt on the UI
 */
type TemplateToken = string|ChipToken;

export class SeaPenTemplateQueryElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-template-query';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      templateId: {
        type: String,
      },

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
    };
  }

  private seaPenTemplate_: SeaPenTemplate;
  private selectedOptions_: Map<string, string>;
  private templateTokens_: TemplateToken[];
  private options_: SeaPenOption[]|null;
  private selectedChip_: ChipToken|null;
  templateId: string|null;

  private computeSeaPenTemplate_(templateId: string|null) {
    const seaPenTemplates = getSampleSeaPenTemplates();
    const correctTemplate = seaPenTemplates.find(
        (seaPenTemplate) => seaPenTemplate.id === templateId);
    return correctTemplate as SeaPenTemplate;
  }

  private isChip_(token: any): token is ChipToken {
    return typeof token?.translation === 'string';
  }

  private onClickChip_(event: Event&{model: {token: ChipToken}}) {
    assert(this.isChip_(event.model.token), 'Token must be a chip');
    this.selectedChip_ = event.model.token;
    assert(
        this.seaPenTemplate_.options.has(this.selectedChip_.id),
        'options must exist');
    this.options_ = this.seaPenTemplate_.options.get(this.selectedChip_.id)!;
  }

  private onClickOption_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const newValue = eventTarget.getAttribute('value') as string;
    // Notifies the selected chip's translation has changed to the UI.
    this.set('selectedChip_.translation', newValue);
    this.selectedOptions_.set(this.selectedChip_!.id, newValue);
    this.templateTokens_ = this.computeTemplateTokens_(
        this.seaPenTemplate_, this.selectedOptions_);
  }

  // TODO(b/309679850): Query for actual images.
  private onClickInspire_() {
    this.seaPenTemplate_.options.forEach((options, chip) => {
      if (isNonEmptyArray(options)) {
        const option = options[getRandomInt(options.length)];
        this.selectedOptions_.set(chip, option.translation);
      } else {
        console.warn('empty options for', this.seaPenTemplate_.id);
        this.selectedOptions_.set(chip, '');
      }
    });
    if (this.selectedChip_) {
      // The selected chip translation might have changed due to randomized
      // option. Notifies the UI to update its value.
      this.set(
          `selectedChip_.translation`,
          this.selectedOptions_.get(this.selectedChip_.id));
    }
    this.templateTokens_ = this.computeTemplateTokens_(
        this.seaPenTemplate_, this.selectedOptions_);
  }

  private onSeaPenTemplateChanged_(template: SeaPenTemplate) {
    const selectedOptions = new Map<string, string>();
    template.options.forEach((options, chip) => {
      if (isNonEmptyArray(options)) {
        const option = options[0];
        selectedOptions.set(chip, option.translation);
      } else {
        console.warn('empty options for', template.id);
        selectedOptions.set(chip, '');
      }
    });
    this.selectedChip_ = null;
    this.options_ = null;
    this.selectedOptions_ = selectedOptions;
    this.templateTokens_ = this.computeTemplateTokens_(
        this.seaPenTemplate_, this.selectedOptions_);
  }

  private computeTemplateTokens_(
      template: SeaPenTemplate, selectedOptions: Map<string, string>) {
    const strs = parseTemplateText(template.text);
    const tokens: TemplateToken[] = [];
    strs.forEach(str => {
      if (isChip(str)) {
        tokens.push({
          translation: isChip(str) ? selectedOptions.get(str) || '' : str,
          id: str,
        });
      } else {
        tokens.push(str);
      }
    });
    return tokens;
  }

  private getChipClassName_(chip: ChipToken, selectedChip: ChipToken|null) {
    assert(this.isChip_(chip), 'Token must be a chip');
    // If there are no selected chips, then use the 'selected' styling on all
    // chips.
    const selected = !selectedChip || chip.id === selectedChip.id ?
        'selected' :
        'unselected';
    return `clickable ${selected}`;
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
}

customElements.define(
    SeaPenTemplateQueryElement.is, SeaPenTemplateQueryElement);
