// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// A <select> element that shows a text input when the "custom" field is
// selected.
export class SelectCustom extends PolymerElement {
  ready() {
    super.ready();
    // Hide custom input.
    this.shadowRoot.querySelector('.hidden-input').hidden = true;

    // Set default value of select to the first option.
    this.shadowRoot.querySelector('#' + this.selectid).selectedIndex = 1;
  }

  static get is() {
    return 'select-custom';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      selectElements: {type: Array},
      selectValue:
          {type: String, notify: true, computed: 'getSelectValue(selectIndex)'},
      selectIndex: {type: Number, value: 0},
      formItemStyle: {type: String, value: 'form-item'},
      displayLabel: {type: String},
      selectid: {type: String},
      noCustomInput: {type: Boolean, value: false},
      customSelected:
          {type: Boolean, value: false, notify: true, reflectToAttribute: true},
    };
  }

  // Shows a hidden input element when the "custom" option is chosen from the
  // select. The function is triggered by the on-change event for the
  // <select> element.
  onSelectChange(event) {
    const customInput = this.shadowRoot.querySelector('.hidden-input');
    if (event.target.value == 'custom' && !this.noCustomInput) {
      this.customSelected = true;
      this.formItemStyle = 'form-item-custom';
      customInput.hidden = false;
    } else {
      this.customSelected = false;
      this.formItemStyle = 'form-item';
      customInput.hidden = true;
      const select = this.shadowRoot.querySelector('#' + this.selectid);
      this.selectIndex = select.selectedIndex;
    }
  }

  // Computed property callback. Returns the value of an element in
  // selectElements using its index.
  getSelectValue(selectIndex) {
    return this.selectElements[selectIndex].value;
  }

  // When the custom input field changes (on-change event), store the value in a
  // property.
  onInputChange() {
    if (this.customSelected) {
      const customInput = this.shadowRoot.querySelector('.hidden-input');
      this.selectValue = customInput.value;
    }
  }
}
customElements.define(SelectCustom.is, SelectCustom);