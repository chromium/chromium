// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './print_preview_vars_css.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, DestinationOrigin} from '../data/destination.js';
// <if expr="chromeos or lacros">
import {ERROR_STRING_KEY_MAP, getPrinterStatusIcon, PrinterStatusReason} from '../data/printer_status_cros.js';
// </if>

import {updateHighlights} from './highlight_utils.js';

// <if expr="chromeos or lacros">
/** @enum {number} */
const DestinationConfigStatus = {
  IDLE: 0,
  IN_PROGRESS: 1,
  FAILED: 2,
};
// </if>

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const PrintPreviewDestinationListItemElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewDestinationListItemElement extends
    PrintPreviewDestinationListItemElementBase {
  static get is() {
    return 'print-preview-destination-list-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Destination} */
      destination: Object,

      /** @type {?RegExp} */
      searchQuery: Object,

      /** @private */
      destinationIcon_: {
        type: String,
        computed: 'computeDestinationIcon_(destination, ' +
            'destination.printerStatusReason)',
      },

      /** @private */
      stale_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** @private {string} */
      searchHint_: String,

      /** @private */
      statusText_: {
        type: String,
        computed:
            'computeStatusText_(destination, destination.printerStatusReason,' +
            'configurationStatus_)',
      },

      // <if expr="chromeos or lacros">
      /** @private */
      isDestinationCrosLocal_: {
        type: Boolean,
        computed: 'computeIsDestinationCrosLocal_(destination)',
        reflectToAttribute: true,
      },

      /** @private {!DestinationConfigStatus} */
      configurationStatus_: {
        type: Number,
        value: DestinationConfigStatus.IDLE,
      },

      /**
       * Mirroring the enum so that it can be used from HTML bindings.
       * @private
       */
      statusEnum_: {
        type: Object,
        value: DestinationConfigStatus,
      },
      // </if>
    };
  }

  static get observers() {
    return [
      'onDestinationPropertiesChange_(' +
          'destination.displayName, destination.isOfflineOrInvalid, ' +
          'destination.isExtension)',
      'updateHighlightsAndHint_(destination, searchQuery)',
      // <if expr="chromeos or lacros">
      'requestPrinterStatus_(destination.key)',
      // </if>
    ];
  }

  constructor() {
    super();

    /** @private {!Array<!Node>} */
    this.highlights_ = [];
  }

  /** @private */
  onDestinationPropertiesChange_() {
    this.title = this.destination.displayName;
    this.stale_ = this.destination.isOfflineOrInvalid;
    if (this.destination.isExtension) {
      const icon = this.shadowRoot.querySelector('.extension-icon');
      icon.style.backgroundImage = '-webkit-image-set(' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/24/1) 1x,' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/48/1) 2x)';
    }
  }

  // <if expr="chromeos or lacros">
  /**
   * Called if the printer configuration request is accepted. Show the waiting
   * message to the user as the configuration might take longer than expected.
   */
  onConfigureRequestAccepted() {
    // It must be a Chrome OS CUPS printer which hasn't been set up before.
    assert(
        this.destination.origin === DestinationOrigin.CROS &&
        !this.destination.capabilities);
    this.configurationStatus_ = DestinationConfigStatus.IN_PROGRESS;
  }

  /**
   * Called when the printer configuration request completes.
   * @param {boolean} success Whether configuration was successful.
   */
  onConfigureComplete(success) {
    this.configurationStatus_ =
        success ? DestinationConfigStatus.IDLE : DestinationConfigStatus.FAILED;
  }

  /**
   * @param {!DestinationConfigStatus} status
   * @return {boolean} Whether the current configuration status is |status|.
   * @private
   */
  checkConfigurationStatus_(status) {
    return this.configurationStatus_ === status;
  }
  // </if>

  /** @private */
  updateHighlightsAndHint_() {
    this.updateSearchHint_();
    removeHighlights(this.highlights_);
    this.highlights_ = updateHighlights(this, this.searchQuery, new Map);
  }

  /** @private */
  updateSearchHint_() {
    const matches = !this.searchQuery ?
        [] :
        this.destination.extraPropertiesToMatch.filter(
            p => p.match(this.searchQuery));
    this.searchHint_ = matches.length === 0 ?
        (this.destination.extraPropertiesToMatch.find(p => !!p) || '') :
        matches.join(' ');
  }

  /**
   * @return {string} A tooltip for the extension printer icon.
   * @private
   */
  getExtensionPrinterTooltip_() {
    if (!this.destination.isExtension) {
      return '';
    }
    return loadTimeData.getStringF(
        'extensionDestinationIconTooltip', this.destination.extensionName);
  }

  /**
   * @return {string} If the destination is a local CrOS printer, this returns
   *    the error text associated with the printer status. For all other
   *    printers this returns the connection status text.
   * @private
   */
  computeStatusText_() {
    if (!this.destination) {
      return '';
    }

    // <if expr="chromeos or lacros">
    if (this.destination.origin === DestinationOrigin.CROS) {
      // Don't show status text when destination is configuring.
      if (this.configurationStatus_ !== DestinationConfigStatus.IDLE) {
        return '';
      }

      const printerStatusReason = this.destination.printerStatusReason;
      if (!printerStatusReason ||
          printerStatusReason === PrinterStatusReason.NO_ERROR ||
          printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
        return '';
      }

      const errorStringKey = ERROR_STRING_KEY_MAP.get(printerStatusReason);
      return errorStringKey ? this.i18n(errorStringKey) : '';
    }
    // </if>

    return this.destination.isOfflineOrInvalid ?
        this.destination.connectionStatusText :
        '';
  }

  /**
   * @return {string}
   * @private
   */
  computeDestinationIcon_() {
    if (!this.destination) {
      return '';
    }

    // <if expr="chromeos or lacros">
    if (this.destination.origin === DestinationOrigin.CROS) {
      return getPrinterStatusIcon(
          this.destination.printerStatusReason,
          this.destination.isEnterprisePrinter);
    }
    // </if>

    return this.destination.icon;
  }

  // <if expr="chromeos or lacros">
  /**
   * True when the destination is a CrOS local printer.
   * @return {boolean}
   * @private
   */
  computeIsDestinationCrosLocal_() {
    return this.destination &&
        this.destination.origin === DestinationOrigin.CROS;
  }

  /** @private */
  requestPrinterStatus_() {
    // Requesting printer status only allowed for local CrOS printers.
    if (this.destination.origin !== DestinationOrigin.CROS) {
      return;
    }

    this.destination.requestPrinterStatus().then(
        destinationKey => this.onPrinterStatusReceived_(destinationKey));
  }

  /**
   * @param {string} destinationKey
   * @private
   */
  onPrinterStatusReceived_(destinationKey) {
    if (this.destination.key === destinationKey) {
      // Notify printerStatusReason to trigger icon and status text update.
      this.notifyPath(`destination.printerStatusReason`);
    }
  }
  // </if>
}

customElements.define(
    PrintPreviewDestinationListItemElement.is,
    PrintPreviewDestinationListItemElement);
