// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-element' bundles functionality safety check elements
 * have in common. It is used by all safety check elements: parent, updates,
 * passwors, etc.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * UI states a safety check child can be in. Defines the basic UI of the child.
 * @enum {number}
 */
export const SafetyCheckIconStatus = {
  RUNNING: 0,
  SAFE: 1,
  INFO: 2,
  WARNING: 3,
};

Polymer({
  is: 'settings-safety-check-child',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Status of the left hand icon.
     * @type {!SafetyCheckIconStatus}
     */
    iconStatus: {
      type: Number,
      value: SafetyCheckIconStatus.RUNNING,
    },

    // Primary label of the child.
    label: String,

    // Secondary label of the child.
    subLabel: String,

    // Text of the right hand button. |null| removes it from the DOM.
    buttonLabel: String,

    // Aria label of the right hand button.
    buttonAriaLabel: String,

    // Classes of the right hand button.
    buttonClass: String,

    // Should the entire row be clickable.
    rowClickable: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    // Is the row directed to external link.
    external: {
      type: Boolean,
      value: false,
    },

    rowClickableIcon_: {
      type: String,
      computed: 'computeRowClickableIcon_(external)',
    },

    // Right hand managed icon. |null| removes it from the DOM.
    managedIcon: String,
  },

  /**
   * Returns the left hand icon for an icon status.
   * @private
   * @return {?string}
   */
  getStatusIcon_: function() {
    switch (this.iconStatus) {
      case SafetyCheckIconStatus.RUNNING:
        return null;
      case SafetyCheckIconStatus.SAFE:
        return 'cr:check';
      case SafetyCheckIconStatus.INFO:
        return 'cr:info';
      case SafetyCheckIconStatus.WARNING:
        return 'cr:warning';
      default:
        assertNotReached();
    }
  },

  /**
   * Returns the left hand icon src for an icon status.
   * @private
   * @return {?string}
   */
  getStatusIconSrc_: function() {
    if (this.iconStatus === SafetyCheckIconStatus.RUNNING) {
      return 'chrome://resources/images/throbber_small.svg';
    }
    return null;
  },

  /**
   * Returns the left hand icon class for an icon status.
   * @private
   * @return {string}
   */
  getStatusIconClass_: function() {
    switch (this.iconStatus) {
      case SafetyCheckIconStatus.RUNNING:
      case SafetyCheckIconStatus.SAFE:
        return 'icon-blue';
      case SafetyCheckIconStatus.WARNING:
        return 'icon-red';
      default:
        return '';
    }
  },

  /**
   * Returns the left hand icon aria label for an icon status.
   * @private
   * @return {string}
   */
  getStatusIconAriaLabel_: function() {
    switch (this.iconStatus) {
      case SafetyCheckIconStatus.RUNNING:
        return this.i18n('safetyCheckIconRunningAriaLabel');
      case SafetyCheckIconStatus.SAFE:
        return this.i18n('safetyCheckIconSafeAriaLabel');
      case SafetyCheckIconStatus.INFO:
        return this.i18n('safetyCheckIconInfoAriaLabel');
      case SafetyCheckIconStatus.WARNING:
        return this.i18n('safetyCheckIconWarningAriaLabel');
      default:
        assertNotReached();
    }
  },

  /**
   * If the right-hand side button should be shown.
   * @private
   * @return {boolean}
   */
  showButton_: function() {
    return !!this.buttonLabel;
  },

  /** @private */
  onButtonClick_: function() {
    this.fire('button-click');
  },

  /**
   * If the right-hand side managed icon should be shown.
   * @private
   * @return {boolean}
   */
  showManagedIcon_: function() {
    return !!this.managedIcon;
  },

  /**
   * Return the icon to show when the row is clickable.
   * @return {string}
   * @private
   */
  computeRowClickableIcon_() {
    return this.external ? 'cr:open-in-new' : 'cr:arrow-right';
  },

  /**
   * Return the subpage role description if the arrow right icon is used.
   * @return {string}
   * @private
   */
  getRoleDescription_() {
    return this.rowClickableIcon_ === 'cr:arrow-right' ?
        this.i18n('subpageArrowRoleDescription') :
        '';
  }
});
