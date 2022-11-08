// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './icons.html.js';
import './option.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {Debouncer, DomRepeatEvent, enqueueDebouncer, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {Action, Option, ViewModel} from './types.js';

export interface CommanderAppElement {
  $: {
    input: HTMLInputElement,
    inputRow: HTMLElement,
  };
}


export class CommanderAppElement extends PolymerElement {
  static get is() {
    return 'commander-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      options_: Array,
      focusedIndex_: {
        type: Number,
        notify: true,
        observer: 'onFocusedIndexChanged_',
      },
      promptText_: String,
      resultSetId_: Number,
    };
  }

  private options_: Option[];
  private focusedIndex_: number;
  private promptText_: string|null;
  private browserProxy_: BrowserProxy;
  private resultSetId_: number|null;
  private savedInput_: string;
  private showNoResults_: boolean;
  private debouncer_: Debouncer|null;

  constructor() {
    super();
    this.browserProxy_ = BrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();
    addWebUiListener('view-model-updated', this.onViewModelUpdated_.bind(this));
    addWebUiListener('initialize', this.initialize_.bind(this));
    this.initialize_();
  }

  /**
   * Resets the UI for a new session.
   */
  private initialize_() {
    this.options_ = [];
    this.$.input.value = '';
    this.$.input.focus();
    this.focusedIndex_ = -1;
    this.resultSetId_ = null;
    this.promptText_ = null;
    this.savedInput_ = '';
    this.showNoResults_ = false;
  }

  private onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.browserProxy_.dismiss();
      return;
    }
    if (e.key === 'ArrowUp') {
      e.preventDefault();
      this.focusedIndex_ = (this.focusedIndex_ + this.options_.length - 1) %
          this.options_.length;
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      this.focusedIndex_ = (this.focusedIndex_ + 1) % this.options_.length;
    } else if (e.key === 'Enter') {
      if (this.focusedIndex_ >= 0 &&
          this.focusedIndex_ < this.options_.length) {
        this.notifySelectedAtIndex_(this.focusedIndex_);
      }
    } else if (
        this.promptText_ && e.key === 'Backspace' &&
        this.$.input.value === '') {
      this.browserProxy_.promptCancelled();
      this.promptText_ = null;
      this.$.input.value = this.savedInput_;
      e.preventDefault();
      this.onInput_();
    }
  }

  private onInput_() {
    this.browserProxy_.textChanged(this.$.input.value);
  }

  private onViewModelUpdated_(viewModel: ViewModel) {
    if (viewModel.action === Action.DISPLAY_RESULTS) {
      this.options_ = viewModel.options || [];
      this.resultSetId_ = viewModel.resultSetId;
      this.showNoResults_ = this.resultSetId_ != null &&
          this.$.input.value !== '' && this.options_.length === 0;
      if (this.options_.length > 0) {
        this.focusedIndex_ = 0;
      }
    } else if (viewModel.action === Action.PROMPT) {
      this.showNoResults_ = false;
      this.options_ = [];
      this.resultSetId_ = viewModel.resultSetId;
      this.promptText_ = viewModel.promptText || null;
      this.savedInput_ = this.$.input.value;
      this.$.input.value = '';
      this.onInput_();
    }
  }

  private onDomChange_() {
    this.debouncer_ = Debouncer.debounce(this.debouncer_, microTask, () => {
      this.browserProxy_.heightChanged(document.body.offsetHeight);
    });
    enqueueDebouncer(this.debouncer_);
  }

  /**
   * Called when a result option is clicked via mouse.
   */
  private onOptionClick_(e: DomRepeatEvent<Option>) {
    this.notifySelectedAtIndex_(e.model.index);
  }

  /**
   *  Used to set `aria-activedescendant` when the focused option changes.
   */
  private onFocusedIndexChanged_() {
    if (this.focusedIndex_ === -1) {
      this.$.inputRow.removeAttribute('aria-activedescendant');
    } else {
      this.$.inputRow.setAttribute(
          'aria-activedescendant', this.getOptionId_(this.focusedIndex_));
    }
  }

  /**
   * Informs the browser that the option at |index| was selected.
   */
  private notifySelectedAtIndex_(index: number) {
    if (this.resultSetId_ !== null) {
      this.browserProxy_.optionSelected(index, this.resultSetId_);
    }
  }

  private getOptionClass_(index: number): string {
    return index === this.focusedIndex_ ? 'focused' : '';
  }

  /**
   * An id is required for aria-activedescendant
   */
  private getOptionId_(index: number): string {
    return 'option-' + index;
  }

  private computeShowChip_(): boolean {
    return this.promptText_ !== null;
  }

  private computeExpanded_(): string {
    return this.options_.length > 0 ? 'true' : 'false';
  }

  private computeAriaSelected_(index: number): string {
    return index === this.focusedIndex_ ? 'true' : 'false';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'commander-app': CommanderAppElement;
  }
}

customElements.define(CommanderAppElement.is, CommanderAppElement);
