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
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {afterNextRender, Debouncer, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SeaPenOption} from './constants.js';
import {SeaPenTemplateChip} from './sea_pen_generated.mojom-webui.js';
import {getTemplate} from './sea_pen_options_element.html.js';
import {ChipToken, isNonEmptyArray} from './sea_pen_utils.js';

const SeaPenOptionsElementBase = I18nMixin(PolymerElement);

export class SeaPenOptionEscapeEvent extends CustomEvent<null> {
  static readonly EVENT_NAME = 'sea-pen-option-escape';

  constructor() {
    super(
        SeaPenOptionEscapeEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: null,
        },
    );
  }
}

export interface SeaPenOptionsElement {
  $: {
    container: HTMLDivElement,
    expandButton: CrButtonElement,
    optionKeys: IronA11yKeysElement,
    optionSelector: IronSelectorElement,
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

      ironSelectedOption_: Object,
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
  private ironSelectedOption_: HTMLElement;

  override connectedCallback() {
    super.connectedCallback();

    window.addEventListener('resize', this.onResized_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    window.removeEventListener('resize', this.onResized_);
  }

  // set focus on the nth option of the option list.
  private focusOnTargetOption_(n: number) {
    const prevButton = this.ironSelectedOption_;
    // Remove focus state of previous option button.
    if (prevButton) {
      prevButton.removeAttribute('tabindex');
    }

    // update focus state on the nth option.
    this.$.optionSelector.selectIndex(n);
    this.ironSelectedOption_.setAttribute('tabindex', '0');
    this.ironSelectedOption_.focus();
  }

  // handle keyboard navigation.
  private onOptionKeyPressed_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.optionSelector;
    const prevButton = this.ironSelectedOption_;

    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        // If the previous item is hidden after pressing 'left' key at the first
        // option or at Expand button, we should navigate to the last visible
        // chip option.
        if (this.isHiddenOptionSelected_()) {
          selector.selectIndex(this.getLastVisibleChipOptionIndex_());
        }
        break;
      case 'right':
        selector.selectNext();
        if (this.isHiddenExpandButtonSelected_()) {
          // If the options are fully expanded and the previous selector is at
          // last chip option, pressing 'right' should navigate to the first
          // chip option.
          selector.selectIndex(0);
        } else if (this.isHiddenChipOptionSelected_()) {
          // If the next option is hidden, select and focus the expand button.
          const expandButton = selector.querySelector('#expandButton');
          selector.selectIndex(selector.indexOf(expandButton!));
        }
        break;
      case 'esc':
        this.dispatchEvent(new SeaPenOptionEscapeEvent());
        return;
      default:
        return;
    }
    // Remove focus state of previous button.
    if (prevButton) {
      prevButton.removeAttribute('tabindex');
    }
    // Add focus state for new button.
    if (this.ironSelectedOption_) {
      this.ironSelectedOption_.setAttribute('tabindex', '0');
      this.ironSelectedOption_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  private onClickOption_(event: Event&{model: {option: SeaPenOption}}) {
    const option = event.model.option;
    // Notifies the selected options has changed to the UI by overriding Polymer
    // dirty check
    this.selectedOptions.set(this.selectedChip!.id, option);
    const copiedSelectedOptions = this.selectedOptions;
    this.selectedOptions = new Map<SeaPenTemplateChip, SeaPenOption>();
    this.selectedOptions = copiedSelectedOptions;
    // Stop the event propagation, otherwise, the event will be passed to parent
    // element (sea pen template query element), onClick_ on template query
    // element will be triggered improperly.
    event.preventDefault();
    event.stopPropagation();
  }

  private isHiddenOptionSelected_() {
    return this.ironSelectedOption_.classList.contains('hidden');
  }

  private isHiddenExpandButtonSelected_() {
    return this.ironSelectedOption_?.id === 'expandButton' &&
        this.isHiddenOptionSelected_();
  }

  private isHiddenChipOptionSelected_() {
    return this.ironSelectedOption_.classList.contains('option') &&
        this.isHiddenOptionSelected_();
  }

  private isSelected_(
      option: SeaPenOption, selectedChip: ChipToken|null,
      selectedOptions: Map<SeaPenTemplateChip, SeaPenOption>): boolean {
    return !!selectedOptions && !!selectedChip &&
        selectedOptions.has(selectedChip.id) &&
        option === selectedOptions.get(selectedChip.id);
  }

  private getLastVisibleChipOptionIndex_(): number {
    const options = this.shadowRoot!.querySelectorAll<CrButtonElement>(
        '.option:not(.hidden)');
    return options.length > 0 ? options.length - 1 : 0;
  }

  private getOptionTabIndex_(
      option: SeaPenOption, selectedChip: ChipToken|null,
      selectedOptions: Map<SeaPenTemplateChip, SeaPenOption>): string {
    return this.isSelected_(option, selectedChip, selectedOptions) ? '0' : '-1';
  }

  private getOptionAriaChecked_(
      option: SeaPenOption, selectedChip: ChipToken|null,
      selectedOptions: Map<SeaPenTemplateChip, SeaPenOption>): string {
    return this.isSelected_(option, selectedChip, selectedOptions).toString();
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

  private onClickExpandButton_(event: Event) {
    this.chipsExpanded_ = true;
    this.shouldShowExpandButton_ = false;
    let firstHiddenIndex = -1;
    this.shadowRoot!.querySelectorAll('.option').forEach((option, index) => {
      if (firstHiddenIndex === -1 && option.classList.contains('hidden')) {
        firstHiddenIndex = index;
      }
      option.classList.remove('hidden');
    });

    if (firstHiddenIndex >= 0) {
      afterNextRender(this, () => {
        // focus on the option that was first hidden before clicking on expand
        // button.
        this.focusOnTargetOption_(firstHiddenIndex);
      });
    }

    // Stop the event propagation, otherwise, the event will be passed to parent
    // element (sea pen template query element), onClick_ on template query
    // element will be triggered improperly.
    event.preventDefault();
    event.stopPropagation();
  }

  private onSelectedChipChanged_() {
    this.chipsExpanded_ = false;
    this.shouldShowExpandButton_ = false;
    afterNextRender(this, () => {
      // Called when the options are fully rendered.
      this.calculateHiddenOptions_();
      // focus on the first option of the list when clicking on a chip.
      this.focusOnTargetOption_(0);
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
