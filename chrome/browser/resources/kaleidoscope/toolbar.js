// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toolbar/cr_toolbar.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class KaleidoscopeToolbarElement extends PolymerElement {
  static get is() {
    return 'kaleidoscope-toolbar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // Sets the tooltip text displayed on the menu button.
      menuLabel: {type: String, value: ''},

      // Sets the text displayed beside the menu button.
      pageName: {type: String, value: ''},

      // Sets the text displayed in the search box.
      searchPrompt: {type: String, value: ''},
    };
  }

  connectedCallback() {
    super.connectedCallback();
    this.hideSearch();

    const toolbar = /** @type {CrToolbarElement} */ (this.$.toolbar);
    const input = toolbar.getSearchField().getSearchInput();
    input.addEventListener('keyup', (e) => {
      if (e.keyCode == 13) {
        const event =
            new CustomEvent('ks-search-updated', {detail: input.value});
        this.dispatchEvent(event);
      }
    });
  }

  hideSearch() {
    const toolbar = /** @type {CrToolbarElement} */ (this.$.toolbar);
    toolbar.getSearchField().style.display = 'none';
  }

  showSearch() {
    const toolbar = /** @type {CrToolbarElement} */ (this.$.toolbar);
    toolbar.getSearchField().style.display = '';
    toolbar.getSearchField().getSearchInput().focus();
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    if (e.detail.length == 0) {
      const event = new CustomEvent('ks-search-cleared');
      this.dispatchEvent(event);
    }
  }
}

customElements.define(
    KaleidoscopeToolbarElement.is, KaleidoscopeToolbarElement);
