// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for ObjectFieldSet, a UI element for displaying the properties
 * of a given Javascript object. These properties are displayed in a fieldset
 * as a series of rows for each key-value pair.
 * Served from chrome://bluetooth-internals/.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './object_fieldset.html.js';

/**
 * A fieldset that lists the properties of a given object. These properties
 * are displayed as a series of rows for each key-value pair.
 * Only the object's own properties are displayed. Boolean values are
 * displayed using images: a green check for 'true', and a red cancel 'x' for
 * 'false'. All other types are converted to their string representation for
 * display.
 */
export class ObjectFieldSetElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  static get is() {
    return 'object-field-set';
  }

  static get observedAttributes() {
    return ['data-value', 'show-all'];
  }

  /** @return {boolean} */
  get showAll() {
    return this.hasAttribute('show-all');
  }

  /** @return {Object} */
  get value() {
    return this.dataset.value ? JSON.parse(this.dataset.value) : null;
  }

  /**
   * Deletes and recreates the table structure with current object data if the
   * object data or "show-all" property have changed.
   */
  attributeChangedCallback(name, oldValue, newValue) {
    assert(name === 'data-value' || name === 'show-all');
    if (newValue === oldValue || !this.dataset.value) {
      return;
    }

    const fieldset = this.shadowRoot.querySelector('fieldset');
    fieldset.innerHTML = trustedTypes.emptyHTML;

    const nameMap = JSON.parse(this.dataset.nameMap);
    const valueObject = JSON.parse(this.dataset.value);
    assert(valueObject);
    Object.keys(valueObject).forEach(function(propName) {
      const value = valueObject[propName];
      if (value === false && !this.showAll) {
        return;
      }

      const name = nameMap[propName] || propName;
      const newField = document.createElement('div');
      newField.classList.add('status');

      const nameDiv = document.createElement('div');
      nameDiv.textContent = name + ':';
      newField.appendChild(nameDiv);

      const valueDiv = document.createElement('div');
      valueDiv.dataset.field = propName;

      if (typeof (value) === 'boolean') {
        valueDiv.classList.add('toggle-status');
        valueDiv.classList.toggle('checked', value);
      } else {
        valueDiv.textContent = String(value);
      }

      newField.appendChild(valueDiv);
      fieldset.appendChild(newField);
    }, this);
  }
}

customElements.define('object-field-set', ObjectFieldSetElement);
