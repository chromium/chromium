// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/js/util.m.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-meta/iron-meta.js';
import './destination_dropdown_cros.js';
import './destination_select_css.js';
import './icons.js';
import './print_preview_shared_css.js';
import './throbber_css.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Base, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudOrigins, Destination, DestinationOrigin, PDF_DESTINATION_KEY, RecentDestination, SAVE_TO_DRIVE_CROS_DESTINATION_KEY} from '../data/destination.js';
import {ERROR_STRING_KEY_MAP, PrinterStatus, PrinterStatusReason, PrinterStatusSeverity} from '../data/printer_status_cros.js';
import {NativeLayer, NativeLayerImpl} from '../native_layer.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {SelectBehavior} from './select_behavior.js';

Polymer({
  is: 'print-preview-destination-select-cros',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  properties: {
    activeUser: String,

    dark: Boolean,

    /** @type {!Destination} */
    destination: Object,

    disabled: Boolean,

    driveDestinationKey: String,

    loaded: Boolean,

    noDestinations: Boolean,

    pdfPrinterDisabled: Boolean,

    /** @type {!Array<!Destination>} */
    recentDestinationList: {
      type: Array,
      observer: 'onRecentDestinationListChanged_',
    },

    /** @private {string} */
    pdfDestinationKey_: {
      type: String,
      value: PDF_DESTINATION_KEY,
    },

    /** @private {string} */
    statusText_: {
      type: String,
      computed:
          'computeStatusText_(destination, destination.printerStatusReason)',
      observer: 'onStatusTextSet_',
    },

    /** @private {string} */
    backgroundImages_: {
      type: String,
      computed:
          'computeBackgroundImages_(destinationIcon_, dark, noDestinations)',
    },

    /** @private {string} */
    destinationIcon_: {
      type: String,
      computed: 'computeDestinationIcon_(selectedValue, destination)',
    },

    /** @private */
    printerStatusFlagEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showPrinterStatus');
      },
      readOnly: true,
    },

    /**
     * The key for this map is a destination.id and the value is a
     * destination.key. This map is needed to track which destinations have had
     * statuses requested while also giving quick look up of destination id to
     * the corresponding destination key.
     * @private {!Map<string, string>}
     */
    statusRequestedMap_: Map,

    /** @private */
    isCurrentDestinationCrosLocal_: {
      type: Boolean,
      computed: 'computeIsCurrentDestinationCrosLocal_(destination)',
      reflectToAttribute: true,
    },

    /** @private */
    saveToDriveFlagEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('printSaveToDrive');
      },
      readOnly: true,
    },

    /** @private */
    driveDestinationKeyCros_: {
      type: String,
      computed:
          'computeDriveDestinationKeyCros_(driveDestinationKey, saveToDriveFlagEnabled_)',
    },
  },

  /** @private {!IronMetaElement} */
  meta_: /** @type {!IronMetaElement} */ (
      Base.create('iron-meta', {type: 'iconset'})),

  /** @override */
  attached() {
    if (!this.printerStatusFlagEnabled_) {
      return;
    }

    this.statusRequestedMap_ = new Map();
  },

  focus() {
    if (this.printerStatusFlagEnabled_) {
      this.$$('#dropdown').$$('#destination-dropdown').focus();
      return;
    }
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
  computeDestinationIcon_() {
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
    // <if expr="chromeos">
    if (keyParams[0] === Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS) {
      return 'print-preview:save-to-drive';
    }
    // </if>
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
  computeBackgroundImages_() {
    if (!this.destinationIcon_) {
      return '';
    }

    let iconSetAndIcon = null;
    if (this.noDestinations) {
      iconSetAndIcon = ['cr', 'error'];
    }
    iconSetAndIcon = iconSetAndIcon || this.destinationIcon_.split(':');
    const iconset = /** @type {!IronIconsetSvgElement} */ (
        this.meta_.byKey(iconSetAndIcon[0]));
    return getSelectDropdownBackground(iconset, iconSetAndIcon[1], this);
  },

  onProcessSelectChange(value) {
    this.fire('selected-option-change', value);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDropdownValueSelected_(e) {
    assert(this.printerStatusFlagEnabled_);

    const selectedItem = e.detail;
    if (!selectedItem || selectedItem.value === this.destination.key) {
      return;
    }

    this.fire('selected-option-change', selectedItem.value);
  },

  /**
   * Send a printer status request for any new destination in the dropdown.
   * @private
   */
  onRecentDestinationListChanged_() {
    if (!this.printerStatusFlagEnabled_) {
      return;
    }

    for (const destination of this.recentDestinationList) {
      if (destination.origin !== DestinationOrigin.CROS ||
          this.statusRequestedMap_.has(destination.id)) {
        continue;
      }

      NativeLayerImpl.getInstance()
          .requestPrinterStatusUpdate(destination.id)
          .then(status => this.onPrinterStatusReceived_(status));
      this.statusRequestedMap_.set(destination.id, destination.key);
    }
  },

  /**
   * Check if the printer in |printerStatus| is currently in the dropdown.
   * Update its status icon if it's present.
   * @param {!PrinterStatus} printerStatus
   * @private
   */
  onPrinterStatusReceived_(printerStatus) {
    assert(this.printerStatusFlagEnabled_);
    if (!printerStatus.printerId) {
      return;
    }

    const destinationKey =
        this.statusRequestedMap_.get(printerStatus.printerId);
    if (!destinationKey) {
      return;
    }

    const indexFound = this.recentDestinationList.findIndex(destination => {
      return destination.id === printerStatus.printerId &&
          destination.origin === DestinationOrigin.CROS;
    });
    if (indexFound === -1) {
      return;
    }

    const statusReason = this.getStatusReasonFromPrinterStatus_(printerStatus);
    if (!statusReason) {
      return;
    }


    this.recentDestinationList[indexFound].printerStatusReason = statusReason;
    // Set the new printer status reason then use notifyPath to trigger the
    // dropdown printer status icons to recalculate their badge color.
    this.notifyPath(`recentDestinationList.${indexFound}.printerStatusReason`);

    // If |printerStatus| is for the currently selected printer, use notifyPath
    // to trigger the destination printer status icon to recalculate its badge
    // color and the destination error status text.
    if (this.destination && this.destination.key === destinationKey) {
      this.notifyPath(`destination.printerStatusReason`);
    }
  },

  /**
   * A |printerStatus| can have multiple status reasons so this function's
   * responsibility is to determine which status reason is most relevant to
   * surface to the user. Any status reason with a severity of WARNING or ERROR
   * will get highest precedence since this usually means the printer is in a
   * bad state. NO_ERROR status reason is the next highest precedence so the
   * printer can be shown as available whenever possible.
   * @param {!PrinterStatus} printerStatus
   * @return {!PrinterStatusReason} Status reason extracted from
   *     |printerStatus|.
   * @private
   */
  getStatusReasonFromPrinterStatus_(printerStatus) {
    assert(this.printerStatusFlagEnabled_);

    if (!printerStatus.printerId) {
      return PrinterStatusReason.UNKNOWN_REASON;
    }

    let seenNoErrorReason = false;
    for (const statusReason of printerStatus.statusReasons) {
      const reason = statusReason.reason;
      const severity = statusReason.severity;

      if (reason !== PrinterStatusReason.UNKNOWN_REASON &&
          (severity === PrinterStatusSeverity.WARNING ||
           severity === PrinterStatusSeverity.ERROR)) {
        return reason;
      }

      if (reason === PrinterStatusReason.NO_ERROR) {
        seenNoErrorReason = true;
      }
    }
    return seenNoErrorReason ? PrinterStatusReason.NO_ERROR :
                               PrinterStatusReason.UNKNOWN_REASON;
  },

  /**
   * @return {string}  An error status for the current destination. If no error
   *     status exists, an empty string.
   * @private
   */
  computeStatusText_: function() {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    // Cloudprint destinations contain their own status text.
    if (CloudOrigins.some(origin => origin === this.destination.origin)) {
      if (this.destination.shouldShowInvalidCertificateError) {
        return this.i18n('noLongerSupportedFragment');
      }
      if (this.destination.connectionStatusText) {
        return this.destination.connectionStatusText;
      }
    }

    if (this.destination.origin !== DestinationOrigin.CROS) {
      return this.destination.shouldShowDeprecatedPrinterWarning ?
          this.i18nAdvanced('printerNotSupportedWarning') :
          '';
    }

    // Only when the flag is enabled do we need to fetch a local printer status
    // error string.
    if (!this.printerStatusFlagEnabled_) {
      return '';
    }

    const printerStatusReason = this.destination.printerStatusReason;
    if (!printerStatusReason ||
        printerStatusReason === PrinterStatusReason.NO_ERROR ||
        printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
      return '';
    }

    return this.getErrorString_(printerStatusReason);
  },

  /** @private */
  onStatusTextSet_() {
    this.$$('#statusText').innerHTML = this.statusText_;
  },

  /**
   * @param {!PrinterStatusReason} printerStatusReason
   * @return {!string}
   * @private
   */
  getErrorString_: function(printerStatusReason) {
    const errorStringKey = ERROR_STRING_KEY_MAP.get(printerStatusReason);
    return errorStringKey ? this.i18n(errorStringKey) : '';
  },

  /**
   * True when the currently selected destination is a CrOS local printer.
   * @return {boolean}
   * @private
   */
  computeIsCurrentDestinationCrosLocal_: function() {
    return this.destination &&
        this.destination.origin === DestinationOrigin.CROS;
  },

  /**
   * Return the options currently visible to the user for testing purposes.
   * @return {!Array<!Element>}
   */
  getVisibleItemsForTest: function() {
    return this.printerStatusFlagEnabled_ ?
        this.$$('#dropdown')
            .shadowRoot.querySelectorAll('.list-item:not([hidden])') :
        this.shadowRoot.querySelectorAll('option:not([hidden])');
  },

  /**
   * @return {string}
   * @private
   */
  computeDriveDestinationKeyCros_: function() {
    return this.saveToDriveFlagEnabled_ ? SAVE_TO_DRIVE_CROS_DESTINATION_KEY :
                                          this.driveDestinationKey;
  }
});
