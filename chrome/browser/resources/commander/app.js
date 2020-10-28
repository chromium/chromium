// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './option.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {CommanderOptionElement} from './option.js';
import {Action, Entity, Option, ViewModel} from './types.js';

export class CommanderAppElement extends PolymerElement {
  static get is() {
    return 'commander-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<!Option>} */
      options_: Array,
      /** @private */
      focusedIndex_: Number,
    };
  }

  constructor() {
    super();
    /** @private {!BrowserProxy} */
    this.browserProxy_ = BrowserProxyImpl.getInstance();

    /** @type {?number} */
    this.resultSetId_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    addWebUIListener('view-model-updated', this.onViewModelUpdated_.bind(this));
    addWebUIListener('initialize', this.initialize_.bind(this));
  }

  /**
   * Resets the UI for a new session.
   * @private
   */
  initialize_() {
    this.options_ = [];
    this.$.input.value = '';
    this.focusedIndex_ = -1;
    this.resultSetId_ = null;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeydown_(e) {
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
    }
  }

  /**
   * @private
   */
  onInput_() {
    this.browserProxy_.textChanged(this.$.input.value);
  }

  /**
   * @param {!ViewModel} viewModel
   * @private
   */
  onViewModelUpdated_(viewModel) {
    if (viewModel.action === Action.DISPLAY_RESULTS) {
      this.options_ = viewModel.options || [];
      this.resultSetId_ = viewModel.resultSetId;
      if (this.options_.length > 0) {
        this.focusedIndex_ = 0;
      }
    }
    // TODO(lgrey): Handle Action.PROMPT
  }

  /** @private */
  onDomChange_() {
    this.browserProxy_.heightChanged(document.body.offsetHeight);
  }

  /**
   * Called when a result option is clicked via mouse.
   * @param {!Event} e
   * @private
   */
  onOptionClick_(e) {
    this.notifySelectedAtIndex_(e.model.index);
  }

  /**
   * Informs the browser that the option at |index| was selected.
   * @param {number} index
   * @private
   */
  notifySelectedAtIndex_(index) {
    if (this.resultSetId_ !== null) {
      this.browserProxy_.optionSelected(
          index, /** type {number} */ this.resultSetId_);
    }
  }

  /**
   * @return {string}
   * @param {number} index
   */
  getOptionClass_(index) {
    return index === this.focusedIndex_ ? 'focused' : '';
  }
}
customElements.define(CommanderAppElement.is, CommanderAppElement);
