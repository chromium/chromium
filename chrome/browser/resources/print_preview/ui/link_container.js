// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './print_preview_vars_css.js';
import './throbber_css.js';

import {isWindows} from 'chrome://resources/js/cr.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, DestinationOrigin} from '../data/destination.js';

Polymer({
  is: 'print-preview-link-container',

  _template: html`{__html_template__}`,

  properties: {
    appKioskMode: Boolean,

    /** @type {?Destination} */
    destination: Object,

    disabled: Boolean,

    /** @private {boolean} */
    shouldShowSystemDialogLink_: {
      type: Boolean,
      computed: 'computeShouldShowSystemDialogLink_(appKioskMode, destination)',
      reflectToAttribute: true,
    },

    /** @private {boolean} */
    systemDialogLinkDisabled_: {
      type: Boolean,
      computed: 'computeSystemDialogLinkDisabled_(disabled)',
    },

    /** @private {boolean} */
    openingSystemDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    openingInPreview_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @return {boolean} Whether the system dialog link should be visible.
   * @private
   */
  computeShouldShowSystemDialogLink_: function() {
    if (this.appKioskMode) {
      return false;
    }
    if (!isWindows) {
      return true;
    }
    return !!this.destination &&
        this.destination.origin == DestinationOrigin.LOCAL &&
        this.destination.id != Destination.GooglePromotedId.SAVE_AS_PDF;
  },

  /**
   * @return {boolean} Whether the system dialog link should be disabled
   * @private
   */
  computeSystemDialogLinkDisabled_: function() {
    return isWindows && this.disabled;
  },

  /** @private */
  onSystemDialogClick_: function() {
    if (!this.shouldShowSystemDialogLink_) {
      return;
    }

    // <if expr="not is_win">
    this.openingSystemDialog_ = true;
    // </if>
    this.fire('print-with-system-dialog');
  },

  // <if expr="is_macosx">
  /** @private */
  onOpenInPreviewClick_: function() {
    this.openingInPreview_ = true;
    this.fire('open-pdf-in-preview');
  },
  // </if>

  /** @return {boolean} Whether the system dialog link is available. */
  systemDialogLinkAvailable: function() {
    return this.shouldShowSystemDialogLink_ && !this.systemDialogLinkDisabled_;
  },
});
