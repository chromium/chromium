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
import {afterNextRender, Debouncer, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SeaPenOption} from './constants.js';
import {SeaPenTemplateChip} from './sea_pen_generated.mojom-webui.js';
import {getTemplate} from './sea_pen_options_element.html.js';
import {ChipToken} from './sea_pen_utils.js';

const SeaPenOptionsElementBase = I18nMixin(PolymerElement);

export interface SeaPenOptionsElement {
  $: {
    options: HTMLDivElement,
  };
}

export class SeaPenOptionsElement extends SeaPenOptionsElementBase {
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
        observer: 'onSelectedChipChanged_',
      },

      selectedOptions: {
        type: Object,
        notify: true,
      },

      chipsExpanded_: {
        type: Boolean,
        value: false,
      },

      shouldShowExpandButton_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private options: SeaPenOption[]|null;
  private selectedChip: ChipToken|null;
  private selectedOptions: Map<SeaPenTemplateChip, SeaPenOption>;
  private chipsExpanded_: boolean;
  private shouldShowExpandButton_: boolean;
  private debouncer_: Debouncer;
  private onResized_: () => void = () => {
    this.debouncer_ =
        Debouncer.debounce(this.debouncer_, timeOut.after(100), () => {
          this.shouldShowExpandButton_ = this.checkWhetherExpandShouldShow_();
        });
  };

  override connectedCallback() {
    super.connectedCallback();

    window.addEventListener('resize', this.onResized_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    window.removeEventListener('resize', this.onResized_);
  }

  private onClickOption_(event: Event&{model: {option: SeaPenOption}}) {
    const option = event.model.option;
    // Notifies the selected options has changed to the UI by overriding Polymer
    // dirty check
    this.selectedOptions.set(this.selectedChip!.id, option);
    const copiedSelectedOptions = this.selectedOptions;
    this.selectedOptions = new Map<SeaPenTemplateChip, SeaPenOption>();
    this.selectedOptions = copiedSelectedOptions;
  }

  private isSelected_(
      option: SeaPenOption, selectedChip: ChipToken|null,
      selectedOptions: Map<SeaPenTemplateChip, SeaPenOption>): boolean {
    return !!selectedOptions && !!selectedChip &&
        selectedOptions.has(selectedChip.id) &&
        option === selectedOptions.get(selectedChip.id);
  }

  private showMoreChips_() {
    this.chipsExpanded_ = true;
    this.shouldShowExpandButton_ = false;
  }

  private checkWhetherExpandShouldShow_(): boolean {
    return !this.chipsExpanded_ &&
        this.$.options.clientHeight < this.$.options.scrollHeight;
  }

  private onSelectedChipChanged_() {
    this.chipsExpanded_ = false;
    this.shouldShowExpandButton_ = false;
    afterNextRender(this, () => {
      this.shouldShowExpandButton_ = this.checkWhetherExpandShouldShow_();
    });
  }

  private getOptionsClassName_(chipsExpanded: boolean): string {
    return chipsExpanded ? 'expanded' : '';
  }
}

customElements.define(SeaPenOptionsElement.is, SeaPenOptionsElement);
