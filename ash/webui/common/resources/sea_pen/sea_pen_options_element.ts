// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays all the options to fill in the
 * template placeholder.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {afterNextRender, Debouncer, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SeaPenOption} from './constants.js';
import {SeaPenTemplateChip} from './sea_pen_generated.mojom-webui.js';
import {getTemplate} from './sea_pen_options_element.html.js';
import {ChipToken, isNonEmptyArray} from './sea_pen_utils.js';

const SeaPenOptionsElementBase = I18nMixin(PolymerElement);

export interface SeaPenOptionsElement {
  $: {
    container: HTMLDivElement,
    expandButton: CrButtonElement,
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
        Debouncer.debounce(this.debouncer_, timeOut.after(50), () => {
          this.calculateHiddenOptions_();
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

  private calculateHiddenOptions_() {
    if (this.chipsExpanded_ || !isNonEmptyArray(this.options)) {
      return;
    }
    this.shouldShowExpandButton_ = true;
    const items = Array.from(
        this.shadowRoot!.querySelectorAll<CrButtonElement>('.option'));
    // Add a placeholder to hold the button width.
    this.$.expandButton.innerText =
        this.i18n('seaPenExpandOptionsButton', items.length);
    const gap = 8;  // 8px gap between chips.
    const expandButtonWidth = this.$.expandButton.clientWidth + gap;

    let row = 1;
    let remainingWidth: number = this.$.container.clientWidth;
    let numHiddenItems: number = 0;
    items.forEach((item, i) => {
      item.classList.remove('hidden');
      const itemWidth = item.clientWidth + gap;
      if (itemWidth <= remainingWidth) {
        remainingWidth -= itemWidth;
      } else {
        // Insufficient space to fit in another chip.
        switch (row) {
          case 1:
            remainingWidth =
                this.$.container.clientWidth - itemWidth - expandButtonWidth;
            row++;
            break;
          case 2:
            // Hide expand button if the last chip can fit in the second row.
            if (i < this.options!.length - 1 ||
                itemWidth > remainingWidth + expandButtonWidth) {
              numHiddenItems = this.options!.length - i;
              item.classList.add('hidden');
            }
            remainingWidth = 0;
            row++;
            break;
          case 3:
            // The number of chips to display may change both ways so we always
            // need to go through the whole list of chips again.
            item.classList.add('hidden');
            break;
        }
      }
    });
    this.shouldShowExpandButton_ = numHiddenItems > 0;
    if (this.shouldShowExpandButton_) {
      this.$.expandButton.innerText =
          this.i18n('seaPenExpandOptionsButton', numHiddenItems);
    }
  }

  private onClickExpandButton_() {
    this.chipsExpanded_ = true;
    this.shouldShowExpandButton_ = false;
    this.shadowRoot!.querySelectorAll('.option').forEach(
        option => option.classList.remove('hidden'));
  }

  private onSelectedChipChanged_() {
    this.chipsExpanded_ = false;
    this.shouldShowExpandButton_ = false;
    afterNextRender(this, () => {
      // Called when the options are fully rendered.
      this.calculateHiddenOptions_();
    });
  }

  private getOptionsClassName_(chipsExpanded: boolean): string {
    return chipsExpanded ? 'expanded' : '';
  }

  private getExpandButtonClassName_(shouldShowExpandButton: boolean): string {
    return shouldShowExpandButton ? '' : 'hidden';
  }
}

customElements.define(SeaPenOptionsElement.is, SeaPenOptionsElement);
