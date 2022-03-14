// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/mwb_shared_icons.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrSearchFieldBehavior} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './tab_search_search_field.html.js';

const TabSearchSearchFieldBase =
    mixinBehaviors([CrSearchFieldBehavior], PolymerElement) as
    {new (): PolymerElement & CrSearchFieldBehavior};

export interface TabSearchSearchField {
  $: {
    inputAnnounce: HTMLElement,
    searchInput: HTMLInputElement,
    searchWrapper: HTMLElement,
  };
}

export class TabSearchSearchField extends TabSearchSearchFieldBase {
  static get is() {
    return 'tab-search-search-field';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Controls autofocus for the search field.
       */
      autofocus: {
        type: Boolean,
        value: false,
      },

      /**
       * Text that describes the resulting tabs currently present in the list.
       */
      searchResultText: {
        type: String,
        value: '',
      },

      shortcut_: {
        type: String,
        value: () => loadTimeData.getString('shortcutText'),
      },

      announceText_: {
        type: String,
        value: '',
      },
    };
  }

  override autofocus: boolean;
  searchResultText: string;
  private shortcut_: string;
  private announceText_: string;

  override getSearchInput(): HTMLInputElement {
    return this.$.searchInput;
  }

  /**
   * Cause a text string to be announced by screen readers. Used for announcing
   * when the input field has focus for better compatibility with screen
   * readers.
   * @param text The text that should be announced.
   */
  announce(text: string) {
    this.$.searchWrapper.append(
        this.$.searchWrapper.removeChild(this.$.inputAnnounce));
    if (this.announceText_ === text) {
      // A timeout is required when announcing duplicate text for certain
      // screen readers.
      this.announceText_ = '';
      setTimeout(() => {
        this.announceText_ = text;
      }, 100);
    } else {
      this.announceText_ = text;
    }
  }

  /**
   * Clear |announceText_| when focus leaves the input field to ensure the text
   * is not re-announced when focus returns to the input field.
   */
  private onInputBlur_() {
    this.announceText_ = '';
  }

  /**
   * Do not schedule the timer from CrSearchFieldBehavior to make search more
   * responsive.
   */
  override onSearchTermInput() {
    this.hasSearchText = this.$.searchInput.value !== '';
    this.getSearchInput().dispatchEvent(
        new CustomEvent('search', {composed: true, detail: this.getValue()}));
  }
}

customElements.define(TabSearchSearchField.is, TabSearchSearchField);
