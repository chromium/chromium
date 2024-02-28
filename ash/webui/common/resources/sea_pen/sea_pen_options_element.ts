// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays all the options to fill in the
 * template placeholder.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SeaPenOption} from './constants.js';
import {SeaPenTemplateChip} from './sea_pen_generated.mojom-webui.js';
import {getTemplate} from './sea_pen_options_element.html.js';
import {ChipToken, isNonEmptyArray} from './sea_pen_utils.js';

export class SeaPenOptionsElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'sea-pen-options';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      options: {
        type: Array,
      },

      selectedChip: {
        type: Object,
      },

      selectedOptions: {
        type: Object,
        notify: true,
      },
    };
  }

private options:
  SeaPenOption[]|null;
private selectedChip:
  ChipToken|null;
private selectedOptions:
  Map<SeaPenTemplateChip, SeaPenOption>;

  private onClickOption_(event: Event&{model: {option: SeaPenOption}}) {
    const option = event.model.option;
    // Notifies the selected options has changed to the UI by overriding Polymer
    // dirty check
    this.selectedOptions.set(this.selectedChip!.id, option);
    const copiedSelectedOptions = this.selectedOptions;
    this.selectedOptions = new Map<SeaPenTemplateChip, SeaPenOption>();
    this.selectedOptions = copiedSelectedOptions;
  }

  private shouldShowOptions_(options: SeaPenOption[]|null): boolean {
    return isNonEmptyArray(options);
  }

  private isSelected_(
      option: SeaPenOption, selectedChip: ChipToken|null,
      selectedOptions: Map<SeaPenTemplateChip, SeaPenOption>): boolean {
    return !!selectedOptions && !!selectedChip &&
        selectedOptions.has(selectedChip.id) &&
        option === selectedOptions.get(selectedChip.id);
  }
}

customElements.define(SeaPenOptionsElement.is, SeaPenOptionsElement);
