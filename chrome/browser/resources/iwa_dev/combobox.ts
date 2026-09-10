// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './combobox.css.js';
import {getHtml} from './combobox.html.js';
import {getCss as getSharedCss} from './shared_style.css.js';

export type ComboboxMatchStrategy = 'prefix'|'contains';

export interface ComboboxOption {
  value: string;
  label?: string;
}

export interface IwaDevComboboxElement {
  $: {
    input: HTMLInputElement,
    clearButton?: CrIconButtonElement,
    dropdownButton?: CrIconButtonElement,
    suggestions?: HTMLElement,
  };
}

export class IwaDevComboboxElement extends CrLitElement {
  static get is() {
    return 'iwa-dev-combobox';
  }

  static override get styles() {
    return [
      getSharedCss(),
      getCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      value: {type: String},
      label: {type: String},
      placeholder: {type: String},
      clearable: {type: Boolean},
      disabled: {type: Boolean, reflect: true},
      readonly: {type: Boolean, reflect: true},
      errorMessage: {type: String, attribute: 'error-message'},
      options: {type: Array},
      matchStrategy: {type: String, attribute: 'match-strategy'},
      showSuggestions_: {type: Boolean, state: true},
      highlightedIndex_: {type: Number, state: true},
      filterQuery_: {type: String, state: true},
    };
  }

  accessor value: string = '';
  accessor label: string = '';
  accessor placeholder: string = '';
  accessor clearable: boolean = false;
  accessor disabled: boolean = false;
  accessor readonly: boolean = false;
  accessor errorMessage: string = '';
  accessor options: ComboboxOption[] = [];
  accessor matchStrategy: ComboboxMatchStrategy = 'prefix';

  protected accessor showSuggestions_: boolean = false;
  protected accessor highlightedIndex_: number = -1;
  protected accessor filterQuery_: string = '';
  private wasDropdownOpen_: boolean = false;

  // Exposes the underlying <input> element, matching cr-input.
  get inputElement(): HTMLInputElement {
    return this.$.input;
  }

  // Overridden to delegate focus to the internal <input> element.
  override focus() {
    this.$.input.focus();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.wasDropdownOpen_ = false;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (!this.isConnected) {
      return;
    }
    const isOpen = this.isDropdownOpen_();
    if (isOpen !== this.wasDropdownOpen_) {
      this.wasDropdownOpen_ = isOpen;
      this.shadowRoot?.querySelector<HTMLElement>('#suggestions')
          ?.togglePopover(isOpen);
    }
  }

  protected getDisplayValue_(): string {
    if (this.readonly) {
      const selectedOption = this.options.find(o => o.value === this.value);
      if (selectedOption?.label) {
        return selectedOption.label;
      }
    }
    return this.value;
  }

  protected getFilteredOptions_(): ComboboxOption[] {
    if (this.readonly || !this.filterQuery_) {
      return this.options;
    }
    const query = this.filterQuery_.toLowerCase();
    const matches = (target?: string): boolean => {
      if (!target) {
        return false;
      }
      const lower = target.toLowerCase();
      return this.matchStrategy === 'contains' ? lower.includes(query) :
                                                 lower.startsWith(query);
    };

    return this.options.filter(opt => matches(opt.value) || matches(opt.label));
  }

  protected isDropdownOpen_(): boolean {
    return this.showSuggestions_ && !this.errorMessage &&
        this.getFilteredOptions_().length > 0;
  }

  /**
   * Returns the ID of the currently highlighted suggestion option for
   * aria-activedescendant, allowing screen readers to announce the active
   * option during keyboard navigation while focus remains on the input.
   */
  protected getActiveDescendant_(): string|undefined {
    return this.isDropdownOpen_() && this.highlightedIndex_ >= 0 ?
        `suggestion-${this.highlightedIndex_}` :
        undefined;
  }

  protected onInput_() {
    if (this.readonly) {
      return;
    }
    this.value = this.$.input.value;
    this.filterQuery_ = this.value.trim().toLowerCase();
    this.errorMessage = '';
    this.showSuggestions_ = true;
    this.highlightedIndex_ = -1;
    this.fireValueChanged_();
  }

  protected onInputPointerdown_(e: PointerEvent) {
    // In readonly mode, clicking anywhere on the input toggles the dropdown
    // (behaving like a native <select> element).
    if (this.readonly) {
      e.preventDefault();
      this.toggleSuggestions_();
    }
  }

  protected onFocus_() {
    if (this.errorMessage) {
      return;
    }
    this.filterQuery_ = '';
    if (!this.readonly && this.options.length > 0) {
      this.showSuggestions_ = true;
    }
  }

  protected onBlur_() {
    this.showSuggestions_ = false;
    this.filterQuery_ = '';
    this.highlightedIndex_ = -1;
  }

  protected onClearClick_() {
    this.value = '';
    this.filterQuery_ = '';
    this.errorMessage = '';
    this.highlightedIndex_ = -1;
    this.$.input.focus();
    this.showSuggestions_ = false;
    this.fireValueChanged_();
  }

  protected onTogglePointerdown_(e: PointerEvent) {
    e.preventDefault();
    this.toggleSuggestions_();
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (this.showSuggestions_) {
      this.onKeydownWhenSuggestionsExpanded_(e);
    } else {
      this.onKeydownWhenSuggestionsCollapsed_(e);
    }
  }

  private onKeydownWhenSuggestionsCollapsed_(e: KeyboardEvent) {
    if ((e.key === 'ArrowDown' || (this.readonly && e.key === ' ')) &&
        this.options.length > 0) {
      e.preventDefault();
      e.stopPropagation();
      this.openSuggestions_();
    }
  }

  private closeSuggestions_() {
    this.showSuggestions_ = false;
    this.highlightedIndex_ = -1;
  }

  private onKeydownWhenSuggestionsExpanded_(e: KeyboardEvent) {
    const filtered = this.getFilteredOptions_();
    const highlighted = filtered[this.highlightedIndex_];

    // In editable mode, allow typing a space character.
    if (e.key === ' ' && (!this.readonly || !highlighted)) {
      return;
    }

    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
        this.navigateHighlight_(e.key === 'ArrowDown' ? 1 : -1, filtered);
        break;
      case 'Enter':
      case ' ':
        if (highlighted) {
          this.selectOption_(highlighted.value);
        } else {
          this.closeSuggestions_();
        }
        break;
      case 'Escape':
        this.closeSuggestions_();
        break;
      default:
        return;
    }

    e.preventDefault();
    e.stopPropagation();
  }

  private openSuggestions_() {
    if (this.options.length === 0) {
      return;
    }
    this.errorMessage = '';
    this.filterQuery_ = '';
    this.showSuggestions_ = true;
    this.highlightedIndex_ = this.getInitialHighlightIndex_(1, this.options);
    this.scrollHighlightedIntoView_();
  }

  private navigateHighlight_(step: 1|- 1, filtered: ComboboxOption[]) {
    if (filtered.length === 0) {
      return;
    }

    this.highlightedIndex_ = this.highlightedIndex_ >= 0 ?
        (this.highlightedIndex_ + step + filtered.length) % filtered.length :
        this.getInitialHighlightIndex_(step, filtered);

    this.scrollHighlightedIntoView_();
  }

  private getInitialHighlightIndex_(step: 1|- 1, options: ComboboxOption[]):
      number {
    const matchIndex = options.findIndex(o => o.value === this.value);
    return matchIndex >= 0 ? matchIndex : (step === 1 ? 0 : options.length - 1);
  }

  private toggleSuggestions_() {
    if (this.showSuggestions_) {
      this.closeSuggestions_();
    } else {
      this.openSuggestions_();
      this.$.input.focus();
    }
  }

  /**
   * Scrolls the highlighted option into view after Lit updates the DOM.
   * Required because browser focus remains on the input, so the browser
   * will not auto-scroll.
   */
  private scrollHighlightedIntoView_() {
    this.updateComplete.then(() => {
      this.shadowRoot?.querySelector('.suggestion-item.highlighted')
          ?.scrollIntoView({block: 'nearest'});
    });
  }

  // Keeps the dropdown open when the user clicks or drags the scrollbar.
  protected onSuggestionsPointerdown_(e: PointerEvent) {
    e.preventDefault();
  }

  protected onOptionPointerdown_(e: PointerEvent) {
    e.preventDefault();
    this.selectOption_((e.currentTarget as HTMLButtonElement).value);
  }

  private selectOption_(val: string) {
    this.value = val;
    this.filterQuery_ = '';
    this.errorMessage = '';
    this.highlightedIndex_ = -1;
    this.$.input.focus();
    this.showSuggestions_ = false;
    this.fireValueChanged_();
  }

  private fireValueChanged_() {
    this.fire('value-changed', {value: this.value});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-combobox': IwaDevComboboxElement;
  }
}

customElements.define(IwaDevComboboxElement.is, IwaDevComboboxElement);
