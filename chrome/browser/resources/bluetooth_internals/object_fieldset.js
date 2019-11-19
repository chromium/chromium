// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for ObjectFieldSet, a UI element for displaying the properties
 * of a given Javascript object. These properties are displayed in a fieldset
 * as a series of rows for each key-value pair.
 * Served from chrome://bluetooth-internals/.
 */

cr.define('object_fieldset', function() {

  /**
   * A fieldset that lists the properties of a given object. These properties
   * are displayed as a series of rows for each key-value pair.
   * Only the object's own properties are displayed. Boolean values are
   * displayed using images: a green check for 'true', and a red cancel 'x' for
   * 'false'. All other types are converted to their string representation for
   * display.
   * @constructor
   * @extends {HTMLFieldSetElement}
   */
  const ObjectFieldSet = cr.ui.define('fieldset');

  ObjectFieldSet.prototype = {
    __proto__: HTMLFieldSetElement.prototype,

    set showAll(showAll) {
      this.showAll_ = showAll;
    },

    get showAll() {
      return this.showAll_;
    },

    /**
     * Decorates the element as an ObjectFieldset.
     */
    decorate: function() {
      this.classList.add('object-fieldset');

      /** @type {?Object} */
      this.value = null;
      /** @private {?Object<string, string>} */
      this.nameMap_ = null;
      this.showAll_ = true;
    },

    /**
     * Sets the object data to be displayed in the fieldset.
     * @param {!Object} value
     */
    setObject: function(value) {
      this.value = value;
      this.redraw();
    },

    /**
     * Sets the object used to map property names to display names. If a display
     * name is not provided, the default property name will be used.
     * @param {!Object<string, string>} nameMap
     */
    setPropertyDisplayNames: function(nameMap) {
      this.nameMap_ = nameMap;
    },

    /**
     * Deletes and recreates the table structure with current object data.
     */
    redraw: function() {
      this.innerHTML = '';

      Object.keys(assert(this.value)).forEach(function(propName) {
        const value = this.value[propName];
        if (value === false && !this.showAll_) {
          return;
        }

        const name = this.nameMap_[propName] || propName;
        const newField = document.createElement('div');
        newField.classList.add('status');

        const nameDiv = document.createElement('div');
        nameDiv.textContent = name + ':';
        newField.appendChild(nameDiv);

        const valueDiv = document.createElement('div');
        valueDiv.dataset.field = propName;

        if (typeof(value) === 'boolean') {
          valueDiv.classList.add('toggle-status');
          valueDiv.classList.toggle('checked', value);
        } else {
          valueDiv.textContent = String(value);
        }

        newField.appendChild(valueDiv);
        this.appendChild(newField);
      }, this);
    },
  };

  return {
    ObjectFieldSet: ObjectFieldSet,
  };
});
