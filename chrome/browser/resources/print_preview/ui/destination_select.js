// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Note: Chrome OS uses print-preview-destination-select-cros rather than the
 * element in this file. Ensure any fixes for cross platform bugs work on both
 * Chrome OS and non-Chrome OS.
 */

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/js/util.m.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-meta/iron-meta.js';
import './destination_select_css.js';
import './icons.js';
import './print_preview_shared_css.js';
import './throbber_css.js';
import '../strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Base, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, DestinationOrigin, PDF_DESTINATION_KEY, RecentDestination} from '../data/destination.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {SelectBehavior} from './select_behavior.js';

Polymer({
  is: 'print-preview-destination-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  properties: {
    activeUser: String,

    dark: Boolean,

    /** @type {!Destination} */
    destination: Object,

    disabled: Boolean,

    loaded: Boolean,

    noDestinations: Boolean,

    pdfPrinterDisabled: Boolean,

    /** @type {!Array<!Destination>} */
    recentDestinationList: Array,

    /** @private {string} */
    pdfDestinationKey_: {
      type: String,
      value: PDF_DESTINATION_KEY,
    },

    /** @private {string} */
    statusText_: {
      type: String,
      computed: 'computeStatusText_(destination)',
      observer: 'onStatusTextSet_'
    },
  },

  /** @private {!IronMetaElement} */
  meta_: /** @type {!IronMetaElement} */ (
      Base.create('iron-meta', {type: 'iconset'})),

  focus() {
    this.$$('.md-select').focus();
  },

  /** Sets the select to the current value of |destination|. */
  updateDestination() {
    this.selectedValue = this.destination.key;
  },

  /**
   * Returns the iconset and icon for the selected printer. If printer details
   * have not yet been retrieved from the backend, attempts to return an
   * appropriate icon early based on the printer's sticky information.
   * @return {string} The iconset and icon for the current selection.
   * @private
   */
  getDestinationIcon_() {
    if (!this.selectedValue) {
      return '';
    }

    // If the destination matches the selected value, pull the icon from the
    // destination.
    if (this.destination && this.destination.key === this.selectedValue) {
      return this.destination.icon;
    }

    // Check for the Docs or Save as PDF ids first.
    const keyParams = this.selectedValue.split('/');
    if (keyParams[0] === Destination.GooglePromotedId.DOCS) {
      return 'print-preview:save-to-drive';
    }
    if (keyParams[0] === Destination.GooglePromotedId.SAVE_AS_PDF) {
      return 'cr:insert-drive-file';
    }

    // Otherwise, must be in the recent list.
    const recent = this.recentDestinationList.find(d => {
      return d.key === this.selectedValue;
    });
    if (recent && recent.icon) {
      return recent.icon;
    }

    // The key/recent destinations don't have information about what icon to
    // use, so just return the generic print icon for now. It will be updated
    // when the destination is set.
    return 'print-preview:print';
  },

  /**
   * @return {string} An inline svg corresponding to the icon for the current
   *     destination and the image for the dropdown arrow.
   * @private
   */
  getBackgroundImages_() {
    const icon = this.getDestinationIcon_();
    if (!icon) {
      return '';
    }

    let iconSetAndIcon = null;
    if (this.noDestinations) {
      iconSetAndIcon = ['cr', 'error'];
    }
    iconSetAndIcon = iconSetAndIcon || icon.split(':');
    const iconset = /** @type {!IronIconsetSvgElement} */ (
        this.meta_.byKey(iconSetAndIcon[0]));
    return getSelectDropdownBackground(iconset, iconSetAndIcon[1], this);
  },

  onProcessSelectChange(value) {
    this.fire('selected-option-change', value);
  },

  /**
   * @return {string} The connection status text to display.
   * @private
   */
  computeStatusText_() {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    if (this.destination.shouldShowInvalidCertificateError) {
      return this.i18n('noLongerSupportedFragment');
    }

    // Give preference to connection status.
    if (this.destination.connectionStatusText) {
      return this.destination.connectionStatusText;
    }

    return '';
  },

  /** @private */
  onStatusTextSet_() {
    this.$$('.destination-status').innerHTML = this.statusText_;
  },

  /**
   * Return the options currently visible to the user for testing purposes.
   * @return {!NodeList<!Element>}
   */
  getVisibleItemsForTest: function() {
    return this.shadowRoot.querySelectorAll('option:not([hidden])');
  }
});
