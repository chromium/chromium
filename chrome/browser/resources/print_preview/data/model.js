// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BackgroundGraphicsModeRestriction, Policies} from '../native_layer.js';
import {Cdd, CddCapabilities, VendorCapability} from './cdd.js';
import {Destination, DestinationOrigin, DestinationType, RecentDestination} from './destination.js';
import {getPrinterTypeForDestination, PrinterType} from './destination_match.js';
// <if expr="chromeos">
import {ColorModeRestriction, DuplexModeRestriction, PinModeRestriction} from './destination_policies.js';
// </if>
import {DocumentSettings} from './document_info.js';
import {CustomMarginsOrientation, Margins, MarginsSetting, MarginsType} from './margins.js';
import {ScalingType} from './scaling.js';
import {Size} from './size.js';

/**
 * |key| is the field in the serialized settings state that corresponds to the
 * setting, or an empty string if the setting should not be saved in the
 * serialized state.
 * @typedef {{
 *   value: *,
 *   unavailableValue: *,
 *   valid: boolean,
 *   available: boolean,
 *   setByPolicy: boolean,
 *   setFromUi: boolean,
 *   key: string,
 *   updatesPreview: boolean,
 * }}
 */
export let Setting;

/**
 * @typedef {{
 *   pages: !Setting,
 *   copies: !Setting,
 *   collate: !Setting,
 *   layout: !Setting,
 *   color: !Setting,
 *   customMargins: !Setting,
 *   mediaSize: !Setting,
 *   margins: !Setting,
 *   dpi: !Setting,
 *   scaling: !Setting,
 *   scalingType: !Setting,
 *   scalingTypePdf: !Setting,
 *   duplex: !Setting,
 *   duplexShortEdge: !Setting,
 *   cssBackground: !Setting,
 *   selectionOnly: !Setting,
 *   headerFooter: !Setting,
 *   rasterize: !Setting,
 *   vendorItems: !Setting,
 *   otherOptions: !Setting,
 *   ranges: !Setting,
 *   pagesPerSheet: !Setting,
 *   pin: (Setting|undefined),
 *   pinValue: (Setting|undefined),
 * }}
 */
export let Settings;

/**
 * @typedef {{
 *    version: number,
 *    recentDestinations: (!Array<!RecentDestination> |
 *                         undefined),
 *    dpi: ({horizontal_dpi: number,
 *           vertical_dpi: number,
 *           is_default: (boolean | undefined)} | undefined),
 *    mediaSize: ({height_microns: number,
 *                 width_microns: number,
 *                 custom_display_name: (string | undefined),
 *                 is_default: (boolean | undefined)} | undefined),
 *    marginsType: (MarginsType | undefined),
 *    customMargins: (MarginsSetting | undefined),
 *    isColorEnabled: (boolean | undefined),
 *    isDuplexEnabled: (boolean | undefined),
 *    isHeaderFooterEnabled: (boolean | undefined),
 *    isLandscapeEnabled: (boolean | undefined),
 *    isCollateEnabled: (boolean | undefined),
 *    isCssBackgroundEnabled: (boolean | undefined),
 *    scaling: (string | undefined),
 *    scalingType: (ScalingType | undefined),
 *    scalingTypePdf: (ScalingType | undefined),
 *    vendor_options: (Object | undefined),
 *    isPinEnabled: (boolean | undefined),
 *    pinValue: (string | undefined)
 * }}
 */
export let SerializedSettings;

/**
 * @typedef {{
 *  value: *,
 *  managed: boolean,
 *  applyOnDestinationUpdate: boolean
 * }}
 */
export let PolicyEntry;

/**
 * @typedef {{
 *   headerFooter: (PolicyEntry | undefined),
 *   cssBackground: (PolicyEntry | undefined),
 *   mediaSize: (PolicyEntry | undefined),
 *   sheets: (number | undefined),
 * }}
 */
export let PolicySettings;

/**
 * Constant values matching printing::DuplexMode enum.
 * @enum {number}
 */
export const DuplexMode = {
  SIMPLEX: 0,
  LONG_EDGE: 1,
  SHORT_EDGE: 2,
  UNKNOWN_DUPLEX_MODE: -1,
};

/**
 * Values matching the types of duplex in a CDD.
 * @enum {string}
 */
export const DuplexType = {
  NO_DUPLEX: 'NO_DUPLEX',
  LONG_EDGE: 'LONG_EDGE',
  SHORT_EDGE: 'SHORT_EDGE'
};

/** @private {?PrintPreviewModelElement} */
let instance = null;

/** @private {!PromiseResolver} */
let whenReadyResolver = new PromiseResolver();

/** @return {!PrintPreviewModelElement} */
export function getInstance() {
  return assert(instance);
}

/** @return {!Promise} */
export function whenReady() {
  return whenReadyResolver.promise;
}

/**
 * Sticky setting names in alphabetical order.
 * @type {!Array<string>}
 */
const STICKY_SETTING_NAMES = [
  'recentDestinations',
  'collate',
  'color',
  'cssBackground',
  'customMargins',
  'dpi',
  'duplex',
  'duplexShortEdge',
  'headerFooter',
  'layout',
  'margins',
  'mediaSize',
  'scaling',
  'scalingType',
  'scalingTypePdf',
  'vendorItems',
];
// <if expr="chromeos">
STICKY_SETTING_NAMES.push('pin', 'pinValue');
// </if>

/**
 * Minimum height of page in microns to allow headers and footers. Should
 * match the value for min_size_printer_units in printing/print_settings.cc
 * so that we do not request header/footer for margins that will be zero.
 * @type {number}
 */
const MINIMUM_HEIGHT_MICRONS = 25400;

Polymer({
  is: 'print-preview-model',

  _template: null,

  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * Initialize all settings to available so that more settings always stays
     * in a collapsed state during startup, when document information and
     * printer capabilities may arrive at slightly different times.
     * @type {!Settings}
     */
    settings: {
      type: Object,
      notify: true,
      value() {
        return {
          pages: {
            value: [1],
            unavailableValue: [],
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: '',
            updatesPreview: false,
          },
          copies: {
            value: 1,
            unavailableValue: 1,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: '',
            updatesPreview: false,
          },
          collate: {
            value: true,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isCollateEnabled',
            updatesPreview: false,
          },
          layout: {
            value: false, /* portrait */
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isLandscapeEnabled',
            updatesPreview: true,
          },
          color: {
            value: true, /* color */
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isColorEnabled',
            updatesPreview: true,
          },
          mediaSize: {
            value: {},
            unavailableValue: {
              width_microns: 215900,
              height_microns: 279400,
            },
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'mediaSize',
            updatesPreview: true,
          },
          margins: {
            value: MarginsType.DEFAULT,
            unavailableValue: MarginsType.DEFAULT,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'marginsType',
            updatesPreview: true,
          },
          customMargins: {
            value: {},
            unavailableValue: {},
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'customMargins',
            updatesPreview: true,
          },
          dpi: {
            value: {},
            unavailableValue: {},
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'dpi',
            updatesPreview: false,
          },
          scaling: {
            value: '100',
            unavailableValue: '100',
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'scaling',
            updatesPreview: true,
          },
          scalingType: {
            value: ScalingType.DEFAULT,
            unavailableValue: ScalingType.DEFAULT,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'scalingType',
            updatesPreview: true,
          },
          scalingTypePdf: {
            value: ScalingType.DEFAULT,
            unavailableValue: ScalingType.DEFAULT,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'scalingTypePdf',
            updatesPreview: true,
          },
          duplex: {
            value: true,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isDuplexEnabled',
            updatesPreview: false,
          },
          duplexShortEdge: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isDuplexShortEdge',
            updatesPreview: false,
          },
          cssBackground: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isCssBackgroundEnabled',
            updatesPreview: true,
          },
          selectionOnly: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: '',
            updatesPreview: true,
          },
          headerFooter: {
            value: true,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isHeaderFooterEnabled',
            updatesPreview: true,
          },
          rasterize: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: '',
            updatesPreview: true,
          },
          vendorItems: {
            value: {},
            unavailableValue: {},
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'vendorOptions',
            updatesPreview: false,
          },
          pagesPerSheet: {
            value: 1,
            unavailableValue: 1,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: '',
            updatesPreview: true,
          },
          // This does not represent a real setting value, and is used only to
          // expose the availability of the other options settings section.
          otherOptions: {
            value: null,
            unavailableValue: null,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: '',
            updatesPreview: false,
          },
          // This does not represent a real settings value, but is used to
          // propagate the correctly formatted ranges for print tickets.
          ranges: {
            value: [],
            unavailableValue: [],
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: '',
            updatesPreview: true,
          },
          recentDestinations: {
            value: [],
            unavailableValue: [],
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'recentDestinations',
            updatesPreview: false,
          },
          // <if expr="chromeos">
          pin: {
            value: false,
            unavailableValue: false,
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'isPinEnabled',
            updatesPreview: false,
          },
          pinValue: {
            value: '',
            unavailableValue: '',
            valid: true,
            available: true,
            setByPolicy: false,
            setFromUi: false,
            key: 'pinValue',
            updatesPreview: false,
          },
          // </if>
        };
      },
    },

    settingsManaged: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /** @type {!Destination} */
    destination: Object,

    /** @type {!DocumentSettings} */
    documentSettings: Object,

    /** @type {Margins} */
    margins: Object,

    /** @type {!Size} */
    pageSize: Object,

    /** @private {number} */
    maxSheets: {
      type: Number,
      value: 0,
      notify: true,
    }
  },

  observers: [
    'updateSettingsFromDestination_(destination.capabilities)',
    'updateSettingsAvailabilityFromDocumentSettings_(' +
        'documentSettings.isModifiable, documentSettings.isFromArc,' +
        'documentSettings.isPdf, documentSettings.hasCssMediaStyles, ' +
        'documentSettings.hasSelection)',
    'updateHeaderFooterAvailable_(' +
        'margins, settings.margins.value, settings.mediaSize.value)',
  ],

  /** @private {boolean} */
  initialized_: false,

  /** @private {?SerializedSettings} */
  stickySettings_: null,

  /** @private {?PolicySettings} */
  policySettings_: null,

  /** @private {?Cdd} */
  lastDestinationCapabilities_: null,

  /** @override */
  attached() {
    assert(!instance);
    instance = this;
    whenReadyResolver.resolve();
  },

  /** @override */
  detached() {
    instance = null;
    whenReadyResolver = new PromiseResolver();
  },

  /**
   * @param {string} settingName Name of the setting to get.
   * @return {Setting} The setting object.
   */
  getSetting(settingName) {
    const setting =
        /** @type {Setting} */ (this.get(settingName, this.settings));
    assert(setting, 'Setting is missing: ' + settingName);
    return setting;
  },

  /**
   * @param {string} settingName Name of the setting to get the value for.
   * @return {*} The value of the setting, accounting for availability.
   */
  getSettingValue(settingName) {
    const setting = this.getSetting(settingName);
    return setting.available ? setting.value : setting.unavailableValue;
  },

  /**
   * Updates settings.settingPath to |value|. Fires a preview-setting-changed
   * event if the modification results in a change to the value returned by
   * getSettingValue().
   * @param {string} settingPath Setting path to set
   * @param {*} value value to set.
   * @private
   */
  setSettingPath_(settingPath, value) {
    const settingName = settingPath.split('.')[0];
    const setting = this.getSetting(settingName);
    const oldValue = this.getSettingValue(settingName);
    this.set(`settings.${settingPath}`, value);
    const newValue = this.getSettingValue(settingName);
    if (newValue !== oldValue && setting.updatesPreview) {
      this.fire('preview-setting-changed');
    }
  },

  /**
   * Sets settings.settingName.value to |value|, unless updating the setting is
   * disallowed by enterprise policy. Fires preview-setting-changed and
   * sticky-setting-changed events if the update impacts the preview or requires
   * an update to sticky settings. Used for setting settings from UI elements.
   * @param {string} settingName Name of the setting to set
   * @param {*} value The value to set the setting to.
   * @param {boolean=} noSticky Whether to avoid stickying the setting. Defaults
   *     to false.
   */
  setSetting(settingName, value, noSticky) {
    const setting = this.getSetting(settingName);
    if (setting.setByPolicy) {
      return;
    }
    const fireStickyEvent = !noSticky && setting.value !== value && setting.key;
    this.setSettingPath_(`${settingName}.value`, value);
    if (!noSticky) {
      this.setSettingPath_(`${settingName}.setFromUi`, true);
    }
    if (fireStickyEvent && this.initialized_) {
      this.fire('sticky-setting-changed', this.getStickySettings_());
    }
  },

  /**
   * @param {string} settingName Name of the setting to set
   * @param {number} start
   * @param {number} end
   * @param {*} newValue The value to add (if any).
   * @param {boolean=} noSticky Whether to avoid stickying the setting. Defaults
   *     to false.
   */
  setSettingSplice(settingName, start, end, newValue, noSticky) {
    const setting = this.getSetting(settingName);
    if (setting.setByPolicy) {
      return;
    }
    if (newValue) {
      this.splice(`settings.${settingName}.value`, start, end, newValue);
    } else {
      this.splice(`settings.${settingName}.value`, start, end);
    }
    if (!noSticky) {
      this.setSettingPath_(`${settingName}.setFromUi`, true);
    }
    if (!noSticky && setting.key && this.initialized_) {
      this.fire('sticky-setting-changed', this.getStickySettings_());
    }
  },

  /**
   * Sets the validity of |settingName| to |valid|. If the validity is changed,
   * fires a setting-valid-changed event.
   * @param {string} settingName Name of the setting to set
   * @param {boolean} valid Whether the setting value is currently valid.
   */
  setSettingValid(settingName, valid) {
    const setting = this.getSetting(settingName);
    // Should not set the setting to invalid if it is not available, as there
    // is no way for the user to change the value in this case.
    if (!valid) {
      assert(setting.available, 'Setting is not available: ' + settingName);
    }
    const shouldFireEvent = valid !== setting.valid;
    this.set(`settings.${settingName}.valid`, valid);
    if (shouldFireEvent) {
      this.fire('setting-valid-changed', valid);
    }
  },

  /**
   * Updates the availability of the settings sections and values of dpi and
   *     media size settings based on the destination capabilities.
   * @private
   */
  updateSettingsFromDestination_() {
    if (!this.destination || !this.settings) {
      return;
    }

    if (this.destination.capabilities === this.lastDestinationCapabilities_) {
      return;
    }

    this.lastDestinationCapabilities_ = this.destination.capabilities;

    const caps = this.destination.capabilities ?
        this.destination.capabilities.printer :
        null;
    this.updateSettingsAvailabilityFromDestination_(caps);

    if (!caps) {
      return;
    }

    this.updateSettingsValues_(caps);
  },

  /**
   * @param {?CddCapabilities} caps The printer capabilities.
   * @private
   */
  updateSettingsAvailabilityFromDestination_(caps) {
    this.setSettingPath_(
        'copies.available', this.destination.hasCopiesCapability);
    this.setSettingPath_('collate.available', !!caps && !!caps.collate);
    this.setSettingPath_(
        'color.available', this.destination.hasColorCapability);

    const capsHasDuplex = !!caps && !!caps.duplex && !!caps.duplex.option;
    const capsHasLongEdge = capsHasDuplex &&
        caps.duplex.option.some(o => o.type === DuplexType.LONG_EDGE);
    const capsHasShortEdge = capsHasDuplex &&
        caps.duplex.option.some(o => o.type === DuplexType.SHORT_EDGE);
    this.setSettingPath_(
        'duplexShortEdge.available', capsHasLongEdge && capsHasShortEdge);
    this.setSettingPath_(
        'duplex.available',
        (capsHasLongEdge || capsHasShortEdge) &&
            caps.duplex.option.some(o => o.type === DuplexType.NO_DUPLEX));

    this.setSettingPath_(
        'vendorItems.available', !!caps && !!caps.vendor_capability);

    // <if expr="chromeos">
    const pinSupported = !!caps && !!caps.pin && !!caps.pin.supported &&
        loadTimeData.getBoolean('isEnterpriseManaged');
    this.set('settings.pin.available', pinSupported);
    this.set('settings.pinValue.available', pinSupported);
    // </if>

    if (this.documentSettings) {
      this.updateSettingsAvailabilityFromDestinationAndDocumentSettings_();
    }
  },

  /** @private */
  updateSettingsAvailabilityFromDestinationAndDocumentSettings_() {
    const isSaveAsPDF = getPrinterTypeForDestination(this.destination) ===
        PrinterType.PDF_PRINTER;
    const knownSizeToSaveAsPdf = isSaveAsPDF &&
        (!this.documentSettings.isModifiable ||
         this.documentSettings.hasCssMediaStyles);
    const scalingAvailable = !knownSizeToSaveAsPdf &&
        !this.documentSettings.isFromArc &&
        (this.documentSettings.isModifiable || this.documentSettings.isPdf);
    this.setSettingPath_('scaling.available', scalingAvailable);
    this.setSettingPath_(
        'scalingType.available',
        scalingAvailable && !this.documentSettings.isPdf);
    this.setSettingPath_(
        'scalingTypePdf.available',
        scalingAvailable && this.documentSettings.isPdf);
    const caps = this.destination && this.destination.capabilities ?
        this.destination.capabilities.printer :
        null;
    this.setSettingPath_(
        'mediaSize.available',
        !!caps && !!caps.media_size && !knownSizeToSaveAsPdf);
    this.setSettingPath_(
        'dpi.available',
        !this.documentSettings.isFromArc && !!caps && !!caps.dpi &&
            !!caps.dpi.option && caps.dpi.option.length > 1);
    this.setSettingPath_('layout.available', this.isLayoutAvailable_(caps));
  },

  /** @private */
  updateSettingsAvailabilityFromDocumentSettings_() {
    if (!this.settings) {
      return;
    }

    this.setSettingPath_(
        'pagesPerSheet.available',
        !this.documentSettings.isFromArc &&
            (this.documentSettings.isModifiable ||
             this.documentSettings.isPdf));
    this.setSettingPath_(
        'margins.available',
        !this.documentSettings.isFromArc && this.documentSettings.isModifiable);
    this.setSettingPath_(
        'customMargins.available',
        !this.documentSettings.isFromArc && this.documentSettings.isModifiable);
    this.setSettingPath_(
        'cssBackground.available',
        !this.documentSettings.isFromArc && this.documentSettings.isModifiable);
    this.setSettingPath_(
        'selectionOnly.available',
        !this.documentSettings.isFromArc &&
            this.documentSettings.isModifiable &&
            this.documentSettings.hasSelection);
    this.setSettingPath_(
        'headerFooter.available',
        !this.documentSettings.isFromArc && this.isHeaderFooterAvailable_());
    this.setSettingPath_(
        'rasterize.available',
        !this.documentSettings.isFromArc &&
            !this.documentSettings.isModifiable && !isWindows && !isMac);
    this.setSettingPath_(
        'otherOptions.available',
        this.settings.cssBackground.available ||
            this.settings.selectionOnly.available ||
            this.settings.headerFooter.available ||
            this.settings.rasterize.available);

    if (this.destination) {
      this.updateSettingsAvailabilityFromDestinationAndDocumentSettings_();
    }
  },

  /** @private */
  updateHeaderFooterAvailable_() {
    if (this.documentSettings === undefined) {
      return;
    }

    this.setSettingPath_(
        'headerFooter.available', this.isHeaderFooterAvailable_());
  },

  /**
   * @return {boolean} Whether the header/footer setting should be available.
   * @private
   */
  isHeaderFooterAvailable_() {
    // Always unavailable for PDFs.
    if (!this.documentSettings.isModifiable) {
      return false;
    }

    // Always unavailable for small paper sizes.
    const microns = this.getSettingValue('layout') ?
        this.getSettingValue('mediaSize').width_microns :
        this.getSettingValue('mediaSize').height_microns;
    if (microns < MINIMUM_HEIGHT_MICRONS) {
      return false;
    }

    // Otherwise, availability depends on the margins.
    const marginsType =
        /** @type {!MarginsType} */ (this.getSettingValue('margins'));
    if (marginsType === MarginsType.NO_MARGINS) {
      return false;
    }

    if (marginsType === MarginsType.MINIMUM) {
      return true;
    }

    return !this.margins ||
        this.margins.get(CustomMarginsOrientation.TOP) > 0 ||
        this.margins.get(CustomMarginsOrientation.BOTTOM) > 0;
  },

  /**
   * @param {?CddCapabilities} caps The printer capabilities.
   * @private
   */
  isLayoutAvailable_(caps) {
    if (!caps || !caps.page_orientation || !caps.page_orientation.option ||
        (!this.documentSettings.isModifiable &&
         !this.documentSettings.isFromArc) ||
        this.documentSettings.hasCssMediaStyles) {
      return false;
    }
    let hasAutoOrPortraitOption = false;
    let hasLandscapeOption = false;
    caps.page_orientation.option.forEach(option => {
      hasAutoOrPortraitOption = hasAutoOrPortraitOption ||
          option.type === 'AUTO' || option.type === 'PORTRAIT';
      hasLandscapeOption = hasLandscapeOption || option.type === 'LANDSCAPE';
    });
    return hasLandscapeOption && hasAutoOrPortraitOption;
  },

  /**
   * @param {?CddCapabilities} caps The printer capabilities.
   * @private
   */
  updateSettingsValues_(caps) {
    if (this.settings.mediaSize.available) {
      const defaultOption = caps.media_size.option.find(o => !!o.is_default) ||
          caps.media_size.option[0];
      let matchingOption = null;
      // If the setting does not have a valid value, the UI has just started so
      // do not try to get a matching value; just set the printer default in
      // case the user doesn't have sticky settings.
      if (this.settings.mediaSize.setFromUi) {
        const currentMediaSize = this.getSettingValue('mediaSize');
        matchingOption = caps.media_size.option.find(o => {
          return o.height_microns === currentMediaSize.height_microns &&
              o.width_microns === currentMediaSize.width_microns;
        });
      }
      this.setSetting('mediaSize', matchingOption || defaultOption, true);
    }

    if (this.settings.dpi.available) {
      const defaultOption =
          caps.dpi.option.find(o => !!o.is_default) || caps.dpi.option[0];
      let matchingOption = null;
      if (this.settings.dpi.setFromUi) {
        const currentDpi = this.getSettingValue('dpi');
        matchingOption = caps.dpi.option.find(o => {
          return o.horizontal_dpi === currentDpi.horizontal_dpi &&
              o.vertical_dpi === currentDpi.vertical_dpi;
        });
      }
      this.setSetting('dpi', matchingOption || defaultOption, true);
    } else if (
        caps && caps.dpi && caps.dpi.option && caps.dpi.option.length > 0) {
      this.setSettingPath_('dpi.unavailableValue', caps.dpi.option[0]);
    }

    if (!this.settings.color.setFromUi && this.settings.color.available) {
      const defaultOption = this.destination.defaultColorOption;
      if (defaultOption) {
        this.setSetting(
            'color',
            !['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'].includes(
                defaultOption.type),
            true);
      }
    } else if (
        !this.settings.color.available &&
        (this.destination.id === Destination.GooglePromotedId.DOCS ||
         this.destination.type === DestinationType.MOBILE)) {
      this.setSettingPath_('color.unavailableValue', true);
    } else if (
        !this.settings.color.available && caps && caps.color &&
        caps.color.option && caps.color.option.length > 0) {
      this.setSettingPath_(
          'color.unavailableValue',
          !['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'].includes(
              caps.color.option[0].type));
    } else if (!this.settings.color.available) {
      // if no color capability is reported, assume black and white.
      this.setSettingPath_('color.unavailableValue', false);
    }

    if (!this.settings.duplex.setFromUi && this.settings.duplex.available) {
      const defaultOption = caps.duplex.option.find(o => !!o.is_default);
      this.setSetting(
          'duplex',
          defaultOption ? (defaultOption.type === DuplexType.LONG_EDGE ||
                           defaultOption.type === DuplexType.SHORT_EDGE) :
                          false,
          true);
      this.setSetting(
          'duplexShortEdge',
          defaultOption ? defaultOption.type === DuplexType.SHORT_EDGE : false,
          true);

      if (!this.settings.duplexShortEdge.available) {
        // Duplex is available, so must have only one two sided printing option.
        // Set duplexShortEdge's unavailable value based on the printer.
        this.setSettingPath_(
            'duplexShortEdge.unavailableValue',
            caps.duplex.option.some(o => o.type === DuplexType.SHORT_EDGE));
      }
    } else if (
        !this.settings.duplex.available && caps && caps.duplex &&
        caps.duplex.option) {
      // In this case, there must only be one option.
      const hasLongEdge =
          caps.duplex.option.some(o => o.type === DuplexType.LONG_EDGE);
      const hasShortEdge =
          caps.duplex.option.some(o => o.type === DuplexType.SHORT_EDGE);
      // If the only option available is long edge, the value should always be
      // true.
      this.setSettingPath_(
          'duplex.unavailableValue', hasLongEdge || hasShortEdge);
      this.setSettingPath_('duplexShortEdge.unavailableValue', hasShortEdge);
    } else if (!this.settings.duplex.available) {
      // If no duplex capability is reported, assume false.
      this.setSettingPath_('duplex.unavailableValue', false);
      this.setSettingPath_('duplexShortEdge.unavailableValue', false);
    }

    if (this.settings.vendorItems.available) {
      const vendorSettings = {};
      for (const item of /** @type {!Array<!VendorCapability>} */ (
               caps.vendor_capability)) {
        let defaultValue = null;
        if (item.type === 'SELECT' && item.select_cap &&
            item.select_cap.option) {
          const defaultOption =
              item.select_cap.option.find(o => !!o.is_default);
          defaultValue = defaultOption ? defaultOption.value : null;
        } else if (item.type === 'RANGE') {
          if (item.range_cap) {
            defaultValue = item.range_cap.default || null;
          }
        } else if (item.type === 'TYPED_VALUE') {
          if (item.typed_value_cap) {
            defaultValue = item.typed_value_cap.default || null;
          }
        }
        if (defaultValue !== null) {
          vendorSettings[item.id] = defaultValue;
        }
      }
      this.setSetting('vendorItems', vendorSettings, true);
    }
  },

  /**
   * Caches the sticky settings and sets up the recent destinations. Sticky
   * settings will be applied when destinaton capabilities have been retrieved.
   * @param {?string} savedSettingsStr The sticky settings from native layer
   */
  setStickySettings(savedSettingsStr) {
    assert(!this.stickySettings_);

    if (!savedSettingsStr) {
      return;
    }

    let savedSettings;
    try {
      savedSettings =
          /** @type {SerializedSettings} */ (JSON.parse(savedSettingsStr));
    } catch (e) {
      console.warn('Unable to parse state ' + e);
      return;  // use default values rather than updating.
    }
    if (savedSettings.version !== 2) {
      return;
    }

    let recentDestinations = savedSettings.recentDestinations || [];
    if (!Array.isArray(recentDestinations)) {
      recentDestinations = [recentDestinations];
    }
    // Initialize recent destinations early so that the destination store can
    // start trying to fetch them.
    this.setSetting('recentDestinations', recentDestinations);

    this.stickySettings_ = savedSettings;
  },

  /**
   * Helper function for configurePolicySetting_(). Sets value and managed flag
   * for given setting.
   * @param {string} settingName Name of the setting being applied.
   * @param {*} value Value of the setting provided via policy.
   * @param {boolean} managed Flag showing whether value of setting is managed.
   * @param {boolean} applyOnDestinationUpdate Flag showing whether policy
   *     should be applied on every destination update.
   * @private
   */
  setPolicySetting_(settingName, value, managed, applyOnDestinationUpdate) {
    if (!this.policySettings_) {
      this.policySettings_ = {};
    }
    this.policySettings_[settingName] = {
      value: value,
      managed: managed,
      applyOnDestinationUpdate: applyOnDestinationUpdate,
    };
  },

  /**
   * Helper function for setPolicySettings(). Calculates value and managed flag
   * of the setting according to allowed and default modes.
   * @param {string} settingName Name of the setting being applied.
   * @param {*} allowedMode Policy value of allowed mode.
   * @param {*} defaultMode Policy value of default mode.
   * @private
   */
  configurePolicySetting_(settingName, allowedMode, defaultMode) {
    switch (settingName) {
      case 'headerFooter': {
        const value = allowedMode !== undefined ? allowedMode : defaultMode;
        if (value !== undefined) {
          this.setPolicySetting_(
              settingName, value, allowedMode !== undefined,
              /*applyOnDestinationUpdate=*/ false);
        }
        break;
      }
      case 'cssBackground': {
        const value = allowedMode ? allowedMode : defaultMode;
        if (value !== undefined) {
          this.setPolicySetting_(
              settingName, value === BackgroundGraphicsModeRestriction.ENABLED,
              !!allowedMode, /*applyOnDestinationUpdate=*/ false);
        }
        break;
      }
      case 'mediaSize': {
        if (defaultMode !== undefined) {
          this.setPolicySetting_(
              settingName, defaultMode, /*managed=*/ false,
              /*applyOnDestinationUpdate=*/ true);
        }
        break;
      }
      default:
        break;
    }
  },

  /**
   * Sets settings in accordance to policies from native code, and prevents
   * those settings from being changed via other means.
   * @param {Policies} policies Value of policies.
   */
  setPolicySettings(policies) {
    if (policies === undefined) {
      return;
    }
    ['headerFooter', 'cssBackground', 'mediaSize'].forEach(settingName => {
      if (!policies[settingName]) {
        return;
      }
      const defaultMode = policies[settingName].defaultMode;
      const allowedMode = policies[settingName].allowedMode;
      this.configurePolicySetting_(settingName, allowedMode, defaultMode);
    });
    // <if expr="chromeos">
    if (policies['sheets']) {
      if (!this.policySettings_) {
        this.policySettings_ = {};
      }
      this.policySettings_['sheets'] = {
        value: policies['sheets'].value,
        applyOnDestinationUpdate: false
      };
    }
    // </if>
  },

  applyStickySettings() {
    if (this.stickySettings_) {
      STICKY_SETTING_NAMES.forEach(settingName => {
        const setting = this.get(settingName, this.settings);
        const value = this.stickySettings_[setting.key];
        if (value !== undefined) {
          this.setSetting(settingName, value);
        } else {
          this.applyScalingStickySettings_(settingName);
        }
      });
    }
    this.applyPolicySettings_();
    this.initialized_ = true;
    this.updateManaged_();
    this.stickySettings_ = null;
    this.fire('sticky-settings-changed', this.getStickySettings_());
  },

  /**
   * Helper function for applyStickySettings(). Checks if the setting
   * is a scaling setting and applies by applying the old types
   * that rely on 'fitToPage' and 'customScaling'.
   * @param {string} settingName Name of the setting being applied.
   * @private
   */
  applyScalingStickySettings_(settingName) {
    // TODO(dhoss): Remove checks for 'customScaling' and 'fitToPage'
    if (settingName === 'scalingType' &&
        'customScaling' in this.stickySettings_) {
      const isCustom = this.stickySettings_['customScaling'];
      const scalingType = isCustom ? ScalingType.CUSTOM : ScalingType.DEFAULT;
      this.setSetting(settingName, scalingType);
    } else if (settingName === 'scalingTypePdf') {
      if ('isFitToPageEnabled' in this.stickySettings_) {
        const isFitToPage = this.stickySettings_['isFitToPageEnabled'];
        const scalingTypePdf = isFitToPage ?
            ScalingType.FIT_TO_PAGE :
            this.getSetting('scalingType').value;
        this.setSetting(settingName, scalingTypePdf);
      } else if (this.getSetting('scalingType').value === ScalingType.CUSTOM) {
        // In the event that 'isFitToPageEnabled' was not in the sticky
        // settings, and 'scalingType' has been set to custom, we want
        // 'scalingTypePdf' to match.
        this.setSetting(settingName, ScalingType.CUSTOM);
      }
    }
  },

  /** @private */
  applyPolicySettings_() {
    if (this.policySettings_) {
      for (const [settingName, policy] of Object.entries(
               this.policySettings_)) {
        // <if expr="chromeos">
        if (settingName === 'sheets') {
          this.maxSheets = this.policySettings_['sheets'].value;
          continue;
        }
        // </if>
        if (policy.value !== undefined && !policy.applyOnDestinationUpdate) {
          this.setSetting(settingName, policy.value, true);
          if (policy.managed) {
            this.set(`settings.${settingName}.setByPolicy`, true);
          }
        }
      }
    }
  },

  // TODO (crbug.com/1069802): Migrate these policies from Destination.policies
  // to NativeInitialSettings.policies.
  /**
   * Restricts settings and applies defaults as defined by policy applicable to
   * current destination.
   */
  applyDestinationSpecificPolicies() {
    // <if expr="chromeos">
    const colorPolicy = this.destination.colorPolicy;
    const colorValue =
        colorPolicy ? colorPolicy : this.destination.defaultColorPolicy;
    if (colorValue) {
      // |this.setSetting| does nothing if policy is present.
      // We want to set the value nevertheless so we call |this.set| directly.
      this.set(
          'settings.color.value', colorValue === ColorModeRestriction.COLOR);
    }
    this.set('settings.color.setByPolicy', !!colorPolicy);

    const duplexPolicy = this.destination.duplexPolicy;
    const duplexValue =
        duplexPolicy ? duplexPolicy : this.destination.defaultDuplexPolicy;
    let setDuplexTypeByPolicy = false;
    if (duplexValue) {
      this.set(
          'settings.duplex.value',
          duplexValue !== DuplexModeRestriction.SIMPLEX);
      if (duplexValue === DuplexModeRestriction.SHORT_EDGE) {
        this.set('settings.duplexShortEdge.value', true);
        setDuplexTypeByPolicy = true;
      } else if (duplexValue === DuplexModeRestriction.LONG_EDGE) {
        this.set('settings.duplexShortEdge.value', false);
        setDuplexTypeByPolicy = true;
      }
    }
    this.set('settings.duplex.setByPolicy', !!duplexPolicy);
    this.set(
        'settings.duplexShortEdge.setByPolicy',
        !!duplexPolicy && setDuplexTypeByPolicy);

    const pinPolicy = this.destination.pinPolicy;
    if (pinPolicy === PinModeRestriction.NO_PIN) {
      this.set('settings.pin.available', false);
      this.set('settings.pinValue.available', false);
    }
    const pinValue = pinPolicy ? pinPolicy : this.destination.defaultPinPolicy;
    if (pinValue) {
      this.set('settings.pin.value', pinValue === PinModeRestriction.PIN);
    }
    this.set('settings.pin.setByPolicy', !!pinPolicy);
    // </if>

    if (this.settings.mediaSize.available && this.policySettings_) {
      const mediaSizePolicy = this.policySettings_['mediaSize'] &&
          this.policySettings_['mediaSize'].value;
      if (mediaSizePolicy !== undefined) {
        const matchingOption =
            this.destination.capabilities.printer.media_size.option.find(o => {
              return o.width_microns === mediaSizePolicy.width &&
                  o.height_microns === mediaSizePolicy.height;
            });
        if (matchingOption !== undefined) {
          this.set('settings.mediaSize.value', matchingOption);
        }
      }
    }

    this.updateManaged_();
  },

  /** @private */
  updateManaged_() {
    let managedSettings = ['cssBackground', 'headerFooter'];
    // <if expr="chromeos">
    managedSettings =
        managedSettings.concat(['color', 'duplex', 'duplexShortEdge', 'pin']);
    // </if>
    this.settingsManaged = managedSettings.some(settingName => {
      const setting = this.getSetting(settingName);
      return setting.available && setting.setByPolicy;
    });
  },

  /** @return {boolean} Whether the model has been initialized. */
  initialized() {
    return this.initialized_;
  },

  /**
   * @return {string} The current serialized settings.
   * @private
   */
  getStickySettings_() {
    const serialization = {
      version: 2,
    };

    STICKY_SETTING_NAMES.forEach(settingName => {
      const setting = this.get(settingName, this.settings);
      if (setting.setFromUi) {
        serialization[assert(setting.key)] = setting.value;
      }
    });

    return JSON.stringify(serialization);
  },

  /**
   * @return {!DuplexMode} The duplex mode selected.
   * @private
   */
  getDuplexMode_() {
    if (!this.getSettingValue('duplex')) {
      return DuplexMode.SIMPLEX;
    }

    return this.getSettingValue('duplexShortEdge') ? DuplexMode.SHORT_EDGE :
                                                     DuplexMode.LONG_EDGE;
  },

  /**
   * @return {!DuplexType} The duplex type selected.
   * @private
   */
  getCddDuplexType_() {
    if (!this.getSettingValue('duplex')) {
      return DuplexType.NO_DUPLEX;
    }

    return this.getSettingValue('duplexShortEdge') ? DuplexType.SHORT_EDGE :
                                                     DuplexType.LONG_EDGE;
  },

  /**
   * Creates a string that represents a print ticket.
   * @param {!Destination} destination Destination to print to.
   * @param {boolean} openPdfInPreview Whether this print request is to open
   *     the PDF in Preview app (Mac only).
   * @param {boolean} showSystemDialog Whether this print request is to show
   *     the system dialog.
   * @return {string} Serialized print ticket.
   */
  createPrintTicket(destination, openPdfInPreview, showSystemDialog) {
    const dpi =
        /**
           @type {{horizontal_dpi: (number | undefined),
                    vertical_dpi: (number | undefined),
                    vendor_id: (number | undefined)}}
         */
        (this.getSettingValue('dpi'));
    const scalingSettingKey = this.getSetting('scalingTypePdf').available ?
        'scalingTypePdf' :
        'scalingType';
    const ticket = {
      mediaSize: this.getSettingValue('mediaSize'),
      pageCount: this.getSettingValue('pages').length,
      landscape: this.getSettingValue('layout'),
      color: destination.getNativeColorModel(
          /** @type {boolean} */ (this.getSettingValue('color'))),
      headerFooterEnabled: false,  // only used in print preview
      marginsType: this.getSettingValue('margins'),
      duplex: this.getDuplexMode_(),
      copies: this.getSettingValue('copies'),
      collate: this.getSettingValue('collate'),
      shouldPrintBackgrounds: this.getSettingValue('cssBackground'),
      shouldPrintSelectionOnly: false,  // only used in print preview
      previewModifiable: this.documentSettings.isModifiable,
      printToGoogleDrive: destination.id === Destination.GooglePromotedId.DOCS,
      printerType: getPrinterTypeForDestination(destination),
      rasterizePDF: this.getSettingValue('rasterize'),
      scaleFactor:
          this.getSettingValue(scalingSettingKey) === ScalingType.CUSTOM ?
          parseInt(this.getSettingValue('scaling'), 10) :
          100,
      scalingType: this.getSettingValue(scalingSettingKey),
      pagesPerSheet: this.getSettingValue('pagesPerSheet'),
      dpiHorizontal: (dpi && 'horizontal_dpi' in dpi) ? dpi.horizontal_dpi : 0,
      dpiVertical: (dpi && 'vertical_dpi' in dpi) ? dpi.vertical_dpi : 0,
      dpiDefault: (dpi && 'is_default' in dpi) ? dpi.is_default : false,
      deviceName: destination.id,
      pageWidth: this.pageSize.width,
      pageHeight: this.pageSize.height,
      showSystemDialog: showSystemDialog,
    };
    // <if expr="chromeos">
    ticket.printToGoogleDrive = ticket.printToGoogleDrive ||
        destination.id === Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS;
    // </if>

    // Set 'cloudPrintID' only if the destination is not local.
    if (!destination.isLocal) {
      ticket.cloudPrintID = destination.id;
    }

    if (this.getSettingValue('margins') === MarginsType.CUSTOM) {
      ticket.marginsCustom = this.getSettingValue('customMargins');
    }

    if (destination.isPrivet || destination.isExtension) {
      // TODO(rbpotter): Get local and PDF printers to use the same ticket and
      // send only this ticket instead of nesting it in a larger ticket.
      ticket.ticket = this.createCloudJobTicket(destination);
      ticket.capabilities = JSON.stringify(destination.capabilities);
    }

    if (openPdfInPreview) {
      ticket.OpenPDFInPreview = true;
    }

    // <if expr="chromeos">
    if (this.getSettingValue('pin')) {
      ticket.pinValue = this.getSettingValue('pinValue');
    }
    if (destination.origin === DestinationOrigin.CROS) {
      ticket.advancedSettings = this.getSettingValue('vendorItems');
    }
    // </if>

    return JSON.stringify(ticket);
  },

  /**
   * Creates an object that represents a Google Cloud Print print ticket.
   * @param {!Destination} destination Destination to print to.
   * @return {string} Google Cloud Print print ticket.
   */
  createCloudJobTicket(destination) {
    assert(
        !destination.isLocal || destination.isPrivet || destination.isExtension,
        'Trying to create a Google Cloud Print print ticket for a local ' +
            ' non-privet and non-extension destination');
    assert(
        destination.capabilities,
        'Trying to create a Google Cloud Print print ticket for a ' +
            'destination with no print capabilities');

    // Create CJT (Cloud Job Ticket)
    const cjt = {version: '1.0', print: {}};
    if (this.settings.collate.available) {
      cjt.print.collate = {collate: this.settings.collate.value};
    }
    if (this.settings.color.available) {
      const selectedOption = destination.getSelectedColorOption(
          /** @type {boolean} */ (this.settings.color.value));
      if (!selectedOption) {
        console.warn('Could not find correct color option');
      } else {
        cjt.print.color = {type: selectedOption.type};
        if (selectedOption.hasOwnProperty('vendor_id')) {
          cjt.print.color.vendor_id = selectedOption.vendor_id;
        }
      }
    } else {
      // Always try setting the color in the print ticket, otherwise a
      // reasonable reader of the ticket will have to do more work, or process
      // the ticket sub-optimally, in order to safely handle the lack of a
      // color ticket item.
      const defaultOption = destination.defaultColorOption;
      if (defaultOption) {
        cjt.print.color = {type: defaultOption.type};
        if (defaultOption.hasOwnProperty('vendor_id')) {
          cjt.print.color.vendor_id = defaultOption.vendor_id;
        }
      }
    }
    if (this.settings.copies.available) {
      cjt.print.copies = {copies: this.getSettingValue('copies')};
    }
    if (this.settings.duplex.available) {
      cjt.print.duplex = {
        type: this.getCddDuplexType_(),
      };
    }
    if (this.settings.mediaSize.available) {
      const mediaValue = this.settings.mediaSize.value;
      cjt.print.media_size = {
        width_microns: mediaValue.width_microns,
        height_microns: mediaValue.height_microns,
        is_continuous_feed: mediaValue.is_continuous_feed,
        vendor_id: mediaValue.vendor_id
      };
    }
    if (!this.settings.layout.available) {
      // In this case "orientation" option is hidden from user, so user can't
      // adjust it for page content, see Landscape.isCapabilityAvailable().
      // We can improve results if we set AUTO here.
      const capability = destination.capabilities.printer ?
          destination.capabilities.printer.page_orientation :
          null;
      if (capability && capability.option &&
          capability.option.some(option => option.type === 'AUTO')) {
        cjt.print.page_orientation = {type: 'AUTO'};
      }
    } else {
      cjt.print.page_orientation = {
        type: this.settings.layout.value ? 'LANDSCAPE' : 'PORTRAIT'
      };
    }
    if (this.settings.dpi.available) {
      const dpiValue = this.settings.dpi.value;
      cjt.print.dpi = {
        horizontal_dpi: dpiValue.horizontal_dpi,
        vertical_dpi: dpiValue.vertical_dpi,
        vendor_id: dpiValue.vendor_id
      };
    }
    if (this.settings.vendorItems.available) {
      const items = this.settings.vendorItems.value;
      cjt.print.vendor_ticket_item = [];
      for (const itemId in items) {
        if (items.hasOwnProperty(itemId)) {
          cjt.print.vendor_ticket_item.push({id: itemId, value: items[itemId]});
        }
      }
    }
    return JSON.stringify(cjt);
  },
});
