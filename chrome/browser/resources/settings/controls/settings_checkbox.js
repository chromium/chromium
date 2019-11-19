// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-checkbox` is a checkbox that controls a supplied preference.
 */
Polymer({
  is: 'settings-checkbox',

  behaviors: [SettingsBooleanControlBehavior],

  properties: {
    /**
     * Alternative source for the sub-label that can contain html markup.
     * Only use with trusted input.
     */
    subLabelHtml: {
      type: String,
      value: '',
      observer: 'onSubLabelHtmlChanged_',
    },
  },

  observers: [
    'onSubLabelChanged_(subLabel, subLabelHtml)',
  ],

  /** @private */
  onSubLabelChanged_: function() {
    this.$.checkbox.ariaDescription = this.$.subLabel.textContent;
  },

  /**
   * Don't let clicks on a link inside the secondary label reach the checkbox.
   * @private
   */
  onSubLabelHtmlChanged_: function() {
    const links = this.root.querySelectorAll('.secondary.label a');
    links.forEach((link) => {
      link.addEventListener('click', this.stopPropagation);
    });
  },

  /**
   * @param {!Event} event
   * @private
   */
  stopPropagation: function(event) {
    event.stopPropagation();
  },

  /**
   * @param {string} subLabel
   * @param {string} subLabelHtml
   * @return {boolean} Whether there is a subLabel
   * @private
   */
  hasSubLabel_: function(subLabel, subLabelHtml) {
    return !!subLabel || !!subLabelHtml;
  },

});
