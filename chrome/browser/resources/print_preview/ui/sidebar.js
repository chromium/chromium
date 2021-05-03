// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './advanced_options_settings.js';
import './button_strip.js';
import './color_settings.js';
import './copies_settings.js';
import './dpi_settings.js';
import './duplex_settings.js';
import './header.js';
import './layout_settings.js';
import './media_size_settings.js';
import './margins_settings.js';
import './more_settings.js';
import './other_options_settings.js';
import './pages_per_sheet_settings.js';
import './pages_settings.js';
// <if expr="chromeos">
import './pin_settings.js';
// </if>
import './print_preview_vars_css.js';
import './scaling_settings.js';
import '../strings.m.js';
// <if expr="not chromeos">
import './link_container.js';
// </if>

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DarkModeBehavior} from '../dark_mode_behavior.js';
import {Destination} from '../data/destination.js';
import {Error, State} from '../data/state.js';
import {Metrics, MetricsContext} from '../metrics.js';

import {DestinationState} from './destination_settings.js';
import {SettingsBehavior} from './settings_behavior.js';

/**
 * Number of settings sections to show when "More settings" is collapsed.
 * @type {number}
 */
const MAX_SECTIONS_TO_SHOW = 6;

Polymer({
  is: 'print-preview-sidebar',

  _template: html`{__html_template__}`,

  behaviors: [
    SettingsBehavior,
    CrContainerShadowBehavior,
    WebUIListenerBehavior,
    DarkModeBehavior,
  ],

  properties: {
    cloudPrintErrorMessage: String,

    controlsManaged: Boolean,

    /** @type {Destination} */
    destination: {
      type: Object,
      notify: true,
    },

    /** @private {!DestinationState} */
    destinationState: {
      type: Number,
      notify: true,
    },

    /** @type {!Error} */
    error: {
      type: Number,
      notify: true,
    },

    isPdf: Boolean,

    pageCount: Number,

    /** @type {!State} */
    state: {
      type: Number,
      observer: 'onStateChanged_',
    },

    /** @private {boolean} */
    controlsDisabled_: {
      type: Boolean,
      computed: 'computeControlsDisabled_(state)',
    },

    maxSheets: Number,

    /** @private {number} */
    sheetCount_: {
      type: Number,
      computed: 'computeSheetCount_(' +
          'settings.pages.*, settings.duplex.*, settings.copies.*)',
    },

    /** @private {boolean} */
    firstLoad_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    isInAppKioskMode_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    settingsExpandedByUser_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    shouldShowMoreSettings_: {
      type: Boolean,
      computed: 'computeShouldShowMoreSettings_(settings.pages.available, ' +
          'settings.copies.available, settings.layout.available, ' +
          'settings.color.available, settings.mediaSize.available, ' +
          'settings.dpi.available, settings.margins.available, ' +
          'settings.pagesPerSheet.available, settings.scaling.available, ' +
          'settings.duplex.available, settings.otherOptions.available, ' +
          'settings.vendorItems.available)',
    },
  },

  /**
   * @param {boolean} appKioskMode
   * @param {string} defaultPrinter The system default printer ID.
   * @param {string} serializedDestinationSelectionRulesStr String with rules
   *     for selecting the default destination.
   * @param {boolean} pdfPrinterDisabled Whether the PDF printer is disabled.
   * @param {boolean} isDriveMounted Whether Google Drive is mounted. Only used
        on Chrome OS.
   */
  init(
      appKioskMode, defaultPrinter, serializedDestinationSelectionRulesStr,
      pdfPrinterDisabled, isDriveMounted) {
    this.isInAppKioskMode_ = appKioskMode;
    pdfPrinterDisabled = this.isInAppKioskMode_ || pdfPrinterDisabled;
    // If PDF printing is disabled, then Save to Drive also needs to be disabled
    // on Chrome OS.
    isDriveMounted = !pdfPrinterDisabled && isDriveMounted;
    this.$.destinationSettings.init(
        defaultPrinter, pdfPrinterDisabled, isDriveMounted,
        serializedDestinationSelectionRulesStr);
  },

  /**
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  computeControlsDisabled_() {
    return this.state !== State.READY;
  },

  /**
   * @return {number} The number of sheets that will be printed.
   * @private
   */
  computeSheetCount_() {
    let sheets = this.getSettingValue('pages').length;
    if (this.getSettingValue('duplex')) {
      sheets = Math.ceil(sheets / 2);
    }
    return sheets * /** @type {number} */ (this.getSettingValue('copies'));
  },

  /**
   * @return {boolean} Whether to show the "More settings" link.
   * @private
   */
  computeShouldShowMoreSettings_() {
    // Destination settings is always available. See if the total number of
    // available sections exceeds the maximum number to show.
    return [
      'pages', 'copies', 'layout', 'color', 'mediaSize', 'margins', 'color',
      'pagesPerSheet', 'scaling', 'dpi', 'duplex', 'otherOptions', 'vendorItems'
    ].reduce((count, setting) => {
      return this.getSetting(setting).available ? count + 1 : count;
    }, 1) > MAX_SECTIONS_TO_SHOW;
  },

  /**
   * @return {boolean} Whether the "more settings" collapse should be expanded.
   * @private
   */
  shouldExpandSettings_() {
    if (this.settingsExpandedByUser_ === undefined ||
        this.shouldShowMoreSettings_ === undefined) {
      return false;
    }

    // Expand the settings if the user has requested them expanded or if more
    // settings is not displayed (i.e. less than 6 total settings available).
    return this.settingsExpandedByUser_ || !this.shouldShowMoreSettings_;
  },

  /** @private */
  onPrintButtonFocused_() {
    this.firstLoad_ = false;
  },

  onStateChanged_() {
    if (this.state !== State.PRINTING) {
      return;
    }

    if (this.shouldShowMoreSettings_) {
      MetricsContext.printSettingsUi().record(
          this.settingsExpandedByUser_ ?
              Metrics.PrintSettingsUiBucket.PRINT_WITH_SETTINGS_EXPANDED :
              Metrics.PrintSettingsUiBucket.PRINT_WITH_SETTINGS_COLLAPSED);
    }
  },

  /** @return {boolean} Whether the system dialog link is available. */
  systemDialogLinkAvailable() {
    const linkContainer = this.$$('print-preview-link-container');
    return !!linkContainer && linkContainer.systemDialogLinkAvailable();
  },
});
