// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'multidevice-radio-button',

  behaviors: [
    CrRadioButtonBehavior,
  ],

  hostAttributes: {'role': 'radio'},

  properties: {
    ariaChecked: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getAriaChecked_(checked)'
    },
    ariaDisabled: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getAriaDisabled_(disabled)'

    },
    ariaLabel: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getLabel_(label)'
    }
  },

  getLabel_(label) {
    return label;
  },

  /**
   * Prevents on-click handles on the control from being activated when the
   * indicator is clicked.
   * @param {!Event} e The click event.
   * @private
   */
  onIndicatorTap_(e) {
    e.preventDefault();
    e.stopPropagation();
  },
});
