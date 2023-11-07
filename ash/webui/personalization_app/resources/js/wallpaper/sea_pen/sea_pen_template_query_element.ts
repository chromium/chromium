// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays template query to search for
 * SeaPen wallpapers.
 */

import {WithPersonalizationStore} from '../../personalization_store.js';
import {isNonEmptyArray} from '../../utils.js';
import {getSampleSeaPenTemplates, parseTemplateText} from '../utils.js';

import {SeaPenOption, SeaPenTemplate} from './sea_pen_collection_element.js';
import {getTemplate} from './sea_pen_template_query_element.html.js';

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
      },

      selectedOptions_: {
        type: Map,
        computed: 'computeSelectedOptions_(seaPenTemplate_)',
      },

      // `templateText_` is the template string. The string is broken down into
      // an array of substrings by whether it contains a word that is a
      // "<chip>".
      templateText_: {
        type: Array,
        computed: 'computeTemplateText_(seaPenTemplate_)',
      },

      // `selectedChip_` is the DOM element of a selected chip in the template
      // string. It is updated whenever the user clicks a chip word in the
      // template.
      selectedChip_: {
        type: HTMLElement,
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
  private templateText_: string[];
  private options_: SeaPenOption[];
  private selectedChip_: HTMLElement;
  templateId: string|null;

  private computeSeaPenTemplate_(templateId: string|null) {
    const seaPenTemplates = getSampleSeaPenTemplates();
    const correctTemplate = seaPenTemplates.find(
        (seaPenTemplate) => seaPenTemplate.id === templateId);
    return correctTemplate as SeaPenTemplate;
  }

  private isChip_(word: string): boolean {
    return !!word && word.startsWith('<');
  }

  private onClickChip_(event: Event) {
    this.selectedChip_ = event.currentTarget as HTMLElement;
    // First class name is the chip. Determined by |this.computeChipClassName_|.
    const chip = `<${this.selectedChip_.className.split(' ')[0]}>`;
    this.options_ = this.seaPenTemplate_.options.get(chip) as SeaPenOption[];
  }

  private onClickOption_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    this.selectedChip_.innerText = eventTarget.innerText;
    // First class name is the chip. Determined by |this.computeChipClassName_|.
    const chip = `<${this.selectedChip_.className.split(' ')[0]}>`;
    const newValue = eventTarget.getAttribute('value') as string;
    this.selectedOptions_.set(chip, newValue);
  }

  private computeSelectedOptions_(template: SeaPenTemplate) {
    const selected = new Map();
    template.options.forEach((options, chip) => {
      selected.set(
          chip, isNonEmptyArray(options) ? options[0].translation : '');
    });
    return selected;
  }

  private getChipDefaultValue_(chip: string) {
    return this.selectedOptions_.get(chip);
  }

  private computeTemplateText_(template: SeaPenTemplate) {
    return parseTemplateText(template.text);
  }

  private computeChipClassName_(chip: string) {
    if (!this.isChip_(chip)) {
      return;
    }
    return `${chip.substring(1, chip.length - 1)} clickable`;
  }
}

customElements.define(
    SeaPenTemplateQueryElement.is, SeaPenTemplateQueryElement);
