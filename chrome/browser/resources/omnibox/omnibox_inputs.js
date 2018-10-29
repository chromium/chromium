// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class OmniboxInputs extends OmniboxElement {
  /** @return {string} */
  static get is() {
    return 'omnibox-inputs';
  }

  constructor() {
    super('omnibox-inputs-template');
  }

  /** @override */
  connectedCallback() {
    this.setupElementListeners_();
  }

  /** @private */
  setupElementListeners_() {
    const onQueryInputsChanged = this.onQueryInputsChanged_.bind(this);
    const onDisplayInputsChagned = this.onDisplayInputsChagned_.bind(this);

    this.$$('input-text').addEventListener('input', onQueryInputsChanged);
    [
      this.$$('prevent-inline-autocomplete'),
      this.$$('prefer-keyword'),
      this.$$('page-classification'),
    ].forEach(elem => elem.addEventListener('change', onQueryInputsChanged));
    [
      this.$$('show-incomplete-results'),
      this.$$('show-details'),
      this.$$('show-all-providers'),
    ].forEach(elem => elem.addEventListener('change', onDisplayInputsChagned));
  }

  /** @private */
  onQueryInputsChanged_() {
    this.dispatchEvent(new CustomEvent('query-inputs-changed', {
      detail: {
        inputText: this.$$('input-text').value,
        cursorPosition: this.$$('input-text').selectionEnd,
        preventInlineAutocomplete: this.$$('prevent-inline-autocomplete').checked,
        preferKeyword: this.$$('prefer-keyword').checked,
        pageClassification: this.$$('page-classification').checked,
      }
    }));
  }

  /** @private */
  onDisplayInputsChagned_() {
    this.dispatchEvent(new CustomEvent('display-inputs-changed'));
  }
}

window.customElements.define(OmniboxInputs.is, OmniboxInputs);
