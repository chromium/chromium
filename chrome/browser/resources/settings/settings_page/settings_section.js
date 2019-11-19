// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * The section can expand vertically to fill its container's padding edge.
 *
 * Example:
 *
 *    <settings-section page-title="[[pageTitle]]" section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-section>
 */

// eslint-disable-next-line prefer-const
let SettingsSectionElement = Polymer({
  is: 'settings-section',

  properties: {
    /**
     * The section name should match a name specified in route.js. The
     * MainPageBehavior will expand this section if this section name matches
     * currentRoute.section.
     */
    section: String,

    /**
     * Title for the section header. Initialize so we can use the
     * getTitleHiddenStatus_ method for accessibility.
     */
    pageTitle: {
      type: String,
      value: '',
    },

    /**
     * A CSS attribute used for temporarily hiding a SETTINGS-SECTION for the
     * purposes of searching.
     */
    hiddenBySearch: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /**
   * Get the value to which to set the aria-hidden attribute of the section
   * heading.
   * @return {boolean|string} A return value of false will not add aria-hidden
   *    while aria-hidden requires a string of 'true' to be hidden as per aria
   *    specs. This function ensures we have the right return type.
   * @private
   */
  getTitleHiddenStatus_: function() {
    return this.pageTitle ? false : 'true';
  },

  focus: function() {
    this.$$('.title').focus();
  }
});
