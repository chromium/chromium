// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Policies} from '../native_layer.js';
import {BackgroundGraphicsModeRestriction} from '../native_layer.js';

// <if expr="is_chromeos">
import {ColorModeRestriction, DuplexModeRestriction, PinModeRestriction} from '../native_layer.js';
// </if>

import type {CapabilityWithReset, Cdd, CddCapabilities, ColorOption, DpiOption, DuplexOption, MediaSizeOption, MediaTypeOption} from './cdd.js';
import type {Destination, RecentDestination} from './destination.js';
import {DestinationOrigin, GooglePromotedDestinationId, PrinterType} from './destination.js';
import type {DocumentSettings} from './document_info.js';
import type {Margins, MarginsSetting} from './margins.js';
import {CustomMarginsOrientation, MarginsType} from './margins.js';

// <if expr="is_chromeos">
import {PrinterStatusReason} from './printer_status_cros.js';
// </if>

import {ScalingType} from './scaling.js';
import type {Size} from './size.js';

/**
 * |key| is the field in the serialized settings state that corresponds to the
 * setting, or an empty string if the setting should not be saved in the
 * serialized state.
 */
export interface Setting {
  value: any;
  unavailableValue: any;
  valid: boolean;
  available: boolean;
  setByPolicy: boolean;
  setFromUi: boolean;
  key: string;
  updatesPreview: boolean;
  policyDefaultValue?: any;
}

export interface Settings {
  pages: Setting;
  copies: Setting;
  collate: Setting;
  layout: Setting;
  color: Setting;
  customMargins: Setting;
  mediaSize: Setting;
  borderless: Setting;
  mediaType: Setting;
  margins: Setting;
  dpi: Setting;
  scaling: Setting;
  scalingType: Setting;
  scalingTypePdf: Setting;
  duplex: Setting;
  duplexShortEdge: Setting;
  cssBackground: Setting;
  selectionOnly: Setting;
  headerFooter: Setting;
  rasterize: Setting;
  vendorItems: Setting;
  otherOptions: Setting;
  ranges: Setting;
  pagesPerSheet: Setting;
  // <if expr="is_chromeos">
  pin: Setting;
  pinValue: Setting;
  // </if>
  recentDestinations: Setting;
}

export interface SerializedSettings {
  version: number;
  recentDestinations?: RecentDestination[];
  dpi?: DpiOption;
  mediaSize?: MediaSizeOption;
  borderless?: boolean;
  mediaType?: MediaTypeOption;
  marginsType?: MarginsType;
  customMargins?: MarginsSetting;
  isColorEnabled?: boolean;
  isDuplexEnabled?: boolean;
  isDuplexShortEdge?: boolean;
  isHeaderFooterEnabled?: boolean;
  isLandscapeEnabled?: boolean;
  isCollateEnabled?: boolean;
  isCssBackgroundEnabled?: boolean;
  scaling?: string;
  scalingType?: ScalingType;
  scalingTypePdf?: ScalingType;
  vendorOptions?: object;
  // <if expr="is_chromeos">
  isPinEnabled?: boolean;
  pinValue?: string;
  // </if>
}

export interface PolicyEntry {
  value: any;
  managed: boolean;
  applyOnDestinationUpdate: boolean;
}

export interface PolicyObjectEntry {
  defaultMode?: any;
  allowedMode?: any;
  value?: number;
}

export interface PolicySettings {
  headerFooter?: PolicyEntry;
  cssBackground?: PolicyEntry;
  mediaSize?: PolicyEntry;
  sheets?: PolicyEntry;
  color?: PolicyEntry;
  duplex?: PolicyEntry;
  pin?: PolicyEntry;
  printPdfAsImageAvailability?: PolicyEntry;
  printPdfAsImage?: PolicyEntry;
}

interface CloudJobTicketPrint {
  page_orientation?: object;
  dpi?: object;
  vendor_ticket_item?: object[];
  copies?: object;
  media_size?: object;
  duplex?: object;
  color?: {vendor_id?: string, type?: string};
  collate?: object;
}

interface CloudJobTicket {
  version: string;
  print: CloudJobTicketPrint;
}

export interface MediaSizeValue {
  width_microns: number;
  height_microns: number;
  imageable_area_left_microns?: number;
  imageable_area_bottom_microns?: number;
  imageable_area_right_microns?: number;
  imageable_area_top_microns?: number;
  has_borderless_variant?: boolean;
}

export interface Ticket {
  collate: boolean;
  color: number;
  copies: number;
  deviceName: string;
  dpiHorizontal: number;
  dpiVertical: number;
  duplex: DuplexMode;
  headerFooterEnabled: boolean;
  landscape: boolean;
  marginsType: MarginsType;
  mediaSize: MediaSizeValue;
  pagesPerSheet: number;
  previewModifiable: boolean;
  printerType: PrinterType;
  rasterizePDF: boolean;
  scaleFactor: number;
  scalingType: ScalingType;
  shouldPrintBackgrounds: boolean;
  shouldPrintSelectionOnly: boolean;
  borderless?: boolean;
  mediaType?: string;
  advancedSettings?: object;
  capabilities?: string;
  marginsCustom?: MarginsSetting;
  openPDFInPreview?: boolean;
  pinValue?: string;
  ticket?: string;
}

export type PrintTicket = Ticket&{
  dpiDefault: boolean,
  pageCount: number,
  pageHeight: number,
  pageWidth: number,
  // <if expr="is_chromeos">
  printerManuallySelected: boolean,
  printToGoogleDrive: boolean,
  // </if>
  showSystemDialog: boolean,
  // <if expr="is_chromeos">
  printerStatusReason?: PrinterStatusReason,
  // </if>
};

/**
 * Constant values matching printing::DuplexMode enum.
 */
export enum DuplexMode {
  SIMPLEX = 0,
  LONG_EDGE = 1,
  SHORT_EDGE = 2,
  UNKNOWN_DUPLEX_MODE = -1,
}

/**
 * Values matching the types of duplex in a CDD.
 */
export enum DuplexType {
  NO_DUPLEX = 'NO_DUPLEX',
  LONG_EDGE = 'LONG_EDGE',
  SHORT_EDGE = 'SHORT_EDGE',
}

let instance: PrintPreviewModelElement|null = null;

let whenReadyResolver: PromiseResolver<void> = new PromiseResolver();

export function getInstance(): PrintPreviewModelElement {
  assert(instance);
  return instance;
}

export function whenReady(): Promise<void> {
  return whenReadyResolver.promise;
}

/**
 * Sticky setting names in alphabetical order.
 */
const STICKY_SETTING_NAMES: Array<keyof Settings> = [
  'recentDestinations',
  'borderless',
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
  'mediaType',
  'scaling',
  'scalingType',
  'scalingTypePdf',
  'vendorItems',
];
// <if expr="is_chromeos">
STICKY_SETTING_NAMES.push('pin', 'pinValue');
// </if>

/**
 * Minimum height of page in microns to allow headers and footers. Should
 * match the value for min_size_printer_units in printing/print_settings.cc
 * so that we do not request header/footer for margins that will be zero.
 */
const MINIMUM_HEIGHT_MICRONS: number = 25400;

// <if expr="is_chromeos">
/**
 * Helper function for configurePolicySetting_(). Calculates default duplex
 * value based on allowed and default policies. Return undefined when both
 * allowed and default duplex policies are not set.
 * @param allowedMode Duplex allowed mode set by policy.
 * @param defaultMode Duplex default mode set by policy.
 */
function getDuplexDefaultValue(
    allowedMode: DuplexModeRestriction|undefined,
    defaultMode: DuplexModeRestriction|undefined): DuplexModeRestriction|
    undefined {
  if (allowedMode !== DuplexModeRestriction.DUPLEX) {
    return (allowedMode === undefined ||
            allowedMode === DuplexModeRestriction.UNSET) ?
        defaultMode :
        allowedMode;
  }

  // If allowedMode === DUPLEX, then we need to use defaultMode as the
  // default value if it's compliant with allowedMode. Other two-sided modes are
  // also available in this case.
  if (defaultMode === DuplexModeRestriction.SHORT_EDGE ||
      defaultMode === DuplexModeRestriction.LONG_EDGE) {
    return defaultMode;
  }

  // In this case defaultMode is either not set or non-compliant with
  // allowedMode. Note that "DUPLEX" is not a single mode, but a group of modes.
  return DuplexModeRestriction.DUPLEX;
}

/**
 * Helper function that checks whether the duplex default value set by policy
 * is supported by a printing destination.
 * @param duplexPolicyDefault Duplex value policy default.
 * @param duplexShortEdgePolicyDefault DuplexShortEdge value policy default.
 * @param caps Capabilities of a printing destination.
 */
function getDuplexPolicyDefaultValueAvailable(
    duplexPolicyDefault: boolean|undefined,
    duplexShortEdgePolicyDefault: boolean|undefined,
    caps: CddCapabilities|null): boolean {
  // `duplexShortEdgePolicyDefault` is undefined if the default mode is set to
  // "Simplex". `duplexPolicyDefault` is defined if and only if there is a
  // default duplex policy.
  if (duplexPolicyDefault === undefined) {
    return false;
  }
  const capsHasDuplexOptions = !!caps && !!caps.duplex && !!caps.duplex.option;
  if (!capsHasDuplexOptions) {
    // There are no duplex capabilities reported by the printer.
    return false;
  }

  let defaultPolicyDuplexType: DuplexType|null = null;
  if (duplexPolicyDefault === false) {
    defaultPolicyDuplexType = DuplexType.NO_DUPLEX;
  } else if (duplexShortEdgePolicyDefault === true) {
    defaultPolicyDuplexType = DuplexType.SHORT_EDGE;
  } else {
    defaultPolicyDuplexType = DuplexType.LONG_EDGE;
  }

  return caps!.duplex!.option.some(o => o.type === defaultPolicyDuplexType);
}
// </if>

function createSettings(): Settings {
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
        imageable_area_left_microns: 0,
        imageable_area_bottom_microns: 0,
        imageable_area_right_microns: 215900,
        imageable_area_top_microns: 279400,
      },
      valid: true,
      available: true,
      setByPolicy: false,
      setFromUi: false,
      key: 'mediaSize',
      updatesPreview: true,
    },
    borderless: {
      value: false,
      unavailableValue: false,
      valid: true,
      available: false,
      setByPolicy: false,
      setFromUi: false,
      key: 'borderless',
      updatesPreview: true,
    },
    mediaType: {
      value: '',
      unavailableValue: '',
      valid: true,
      available: false,
      setByPolicy: false,
      setFromUi: false,
      key: 'mediaType',
      updatesPreview: false,
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
    // <if expr="is_chromeos">
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
}


export class PrintPreviewModelElement extends PolymerElement {
  static get is() {
    return 'print-preview-model';
  }

  static get template() {
    return null;
  }

  static get properties() {
    return {
      /**
       * Object containing current settings of Print Preview, for use by Polymer
       * controls.
       * Initialize all settings to available so that more settings always stays
       * in a collapsed state during startup, when document information and
       * printer capabilities may arrive at slightly different times.
       */
      settings: {
        type: Object,
        notify: true,
        value: () => createSettings(),
      },

      settingsManaged: {
        type: Boolean,
        notify: true,
        value: false,
      },

      destination: Object,

      documentSettings: Object,

      margins: Object,

      pageSize: Object,

      maxSheets: {
        type: Number,
        value: 0,
        notify: true,
      },
    };
  }

  static get observers() {
    return [
      'updateSettingsFromDestination_(destination.capabilities)',
      'updateSettingsAvailabilityFromDocumentSettings_(' +
          'documentSettings.isModifiable, documentSettings.isFromArc,' +
          'documentSettings.allPagesHaveCustomSize,' +
          'documentSettings.allPagesHaveCustomOrientation,' +
          'documentSettings.hasSelection)',
      'updateHeaderFooterAvailable_(' +
          'margins, settings.margins.value, settings.mediaSize.value)',

    ];
  }

  settings: Settings;
  settingsManaged: boolean;
  destination: Destination;
  documentSettings: DocumentSettings;
  margins: Margins;
  pageSize: Size;
  maxSheets: number;

  private initialized_: boolean = false;
  private stickySettings_: SerializedSettings|null = null;
  private policySettings_: PolicySettings|null = null;
  private lastDestinationCapabilities_: Cdd|null = null;

  override connectedCallback() {
    super.connectedCallback();

    assert(!instance);
    instance = this;
    whenReadyResolver.resolve();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    instance = null;
    whenReadyResolver = new PromiseResolver();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  getSetting(settingName: keyof Settings): Setting {
    const setting = (this.get(settingName, this.settings) as Setting);
    assert(setting, 'Setting is missing: ' + settingName);
    return setting;
  }

  /**
   * @param settingName Name of the setting to get the value for.
   * @return The value of the setting, accounting for availability.
   */
  getSettingValue(settingName: keyof Settings): any {
    const setting = this.getSetting(settingName);
    return setting.available ? setting.value : setting.unavailableValue;
  }

  /**
   * Updates settings.settingPath to |value|. Fires a preview-setting-changed
   * event if the modification results in a change to the value returned by
   * getSettingValue().
   */
  private setSettingPath_(settingPath: string, value: any) {
    const settingName = settingPath.split('.')[0] as keyof Settings;
    const setting = this.getSetting(settingName);
    const oldValue = this.getSettingValue(settingName);
    this.set(`settings.${settingPath}`, value);
    const newValue = this.getSettingValue(settingName);
    if (newValue !== oldValue && setting.updatesPreview) {
      this.fire_('preview-setting-changed');
    }
  }

  /**
   * Sets settings.settingName.value to |value|, unless updating the setting is
   * disallowed by enterprise policy. Fires preview-setting-changed and
   * sticky-setting-changed events if the update impacts the preview or requires
   * an update to sticky settings. Used for setting settings from UI elements.
   * @param settingName Name of the setting to set
   * @param value The value to set the setting to.
   * @param noSticky Whether to avoid stickying the setting. Defaults to false.
   */
  setSetting(settingName: keyof Settings, value: any, noSticky?: boolean) {
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
      this.fire_('sticky-setting-changed', this.getStickySettings_());
    }
  }

  /**
   * @param settingName Name of the setting to set
   * @param start
   * @param end
   * @param newValue The value to add (if any).
   * @param noSticky Whether to avoid stickying the setting. Defaults to false.
   */
  setSettingSplice(
      settingName: keyof Settings, start: number, end: number, newValue: any,
      noSticky?: boolean) {
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
      this.fire_('sticky-setting-changed', this.getStickySettings_());
    }
  }

  /**
   * Sets the validity of |settingName| to |valid|. If the validity is changed,
   * fires a setting-valid-changed event.
   * @param settingName Name of the setting to set
   * @param valid Whether the setting value is currently valid.
   */
  setSettingValid(settingName: keyof Settings, valid: boolean) {
    const setting = this.getSetting(settingName);
    // Should not set the setting to invalid if it is not available, as there
    // is no way for the user to change the value in this case.
    if (!valid) {
      assert(setting.available, 'Setting is not available: ' + settingName);
    }
    const shouldFireEvent = valid !== setting.valid;
    this.set(`settings.${settingName}.valid`, valid);
    if (shouldFireEvent) {
      this.fire_('setting-valid-changed', valid);
    }
  }

  /**
   * Updates the availability of the settings sections and values of dpi and
   *     media size settings based on the destination capabilities.
   */
  private updateSettingsFromDestination_() {
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
    this.applyPersistentCddDefaults_();
  }

  private updateSettingsAvailabilityFromDestination_(caps: CddCapabilities|
                                                     null) {
    this.setSettingPath_(
        'copies.available', this.destination.hasCopiesCapability);
    this.setSettingPath_('collate.available', !!caps && !!caps.collate);
    this.setSettingPath_(
        'color.available', this.destination.hasColorCapability);

    const capsHasDuplex = !!caps && !!caps.duplex && !!caps.duplex.option;
    const capsHasLongEdge = capsHasDuplex &&
        caps!.duplex!.option.some(o => o.type === DuplexType.LONG_EDGE);
    const capsHasShortEdge = capsHasDuplex &&
        caps!.duplex!.option.some(o => o.type === DuplexType.SHORT_EDGE);
    this.setSettingPath_(
        'duplexShortEdge.available', capsHasLongEdge && capsHasShortEdge);
    this.setSettingPath_(
        'duplex.available',
        (capsHasLongEdge || capsHasShortEdge) &&
            caps!.duplex!.option.some(o => o.type === DuplexType.NO_DUPLEX));

    this.setSettingPath_(
        'vendorItems.available', !!caps && !!caps.vendor_capability);

    // <if expr="is_chromeos">
    const pinSupported = !!caps && !!caps.pin && !!caps.pin.supported &&
        loadTimeData.getBoolean('isEnterpriseManaged');
    this.set('settings.pin.available', pinSupported);
    this.set('settings.pinValue.available', pinSupported);
    // </if>

    if (this.documentSettings) {
      this.updateSettingsAvailabilityFromDestinationAndDocumentSettings_();
    }
  }

  private updateSettingsAvailabilityFromDestinationAndDocumentSettings_() {
    const isSaveAsPDF = this.destination.type === PrinterType.PDF_PRINTER;
    const knownSizeToSaveAsPdf = isSaveAsPDF &&
        (!this.documentSettings.isModifiable ||
         this.documentSettings.allPagesHaveCustomSize);
    const scalingAvailable =
        !knownSizeToSaveAsPdf && !this.documentSettings.isFromArc;
    this.setSettingPath_('scaling.available', scalingAvailable);
    this.setSettingPath_(
        'scalingType.available',
        scalingAvailable && this.documentSettings.isModifiable);
    this.setSettingPath_(
        'scalingTypePdf.available',
        scalingAvailable && !this.documentSettings.isModifiable);
    const caps = this.destination && this.destination.capabilities ?
        this.destination.capabilities.printer :
        null;
    this.setSettingPath_(
        'mediaSize.available',
        !!caps && !!caps.media_size && !knownSizeToSaveAsPdf);
    this.setSettingPath_(
        'borderless.available', this.isBorderlessAvailable_(caps));
    this.setSettingPath_(
        'mediaType.available',
        loadTimeData.getBoolean('isBorderlessPrintingEnabled') && !!caps &&
            !!caps.media_type && !!caps.media_type.option &&
            caps.media_type.option.length > 1);
    this.setSettingPath_(
        'dpi.available',
        !this.documentSettings.isFromArc && !!caps && !!caps.dpi &&
            !!caps.dpi.option && caps.dpi.option.length > 1);
    this.setSettingPath_('layout.available', this.isLayoutAvailable_(caps));
  }

  private updateSettingsAvailabilityFromDocumentSettings_() {
    if (!this.settings) {
      return;
    }

    this.setSettingPath_(
        'pagesPerSheet.available', !this.documentSettings.isFromArc);
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
        !this.documentSettings.isFromArc && this.isRasterizeAvailable_());
    this.setSettingPath_(
        'otherOptions.available',
        this.settings.cssBackground.available ||
            this.settings.selectionOnly.available ||
            this.settings.headerFooter.available ||
            this.settings.rasterize.available);

    if (this.destination) {
      this.updateSettingsAvailabilityFromDestinationAndDocumentSettings_();
    }
  }

  private updateHeaderFooterAvailable_() {
    if (this.documentSettings === undefined) {
      return;
    }

    this.setSettingPath_(
        'headerFooter.available', this.isHeaderFooterAvailable_());
  }

  /**
   * @return Whether the header/footer setting should be available.
   */
  private isHeaderFooterAvailable_(): boolean {
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
    const marginsType = this.getSettingValue('margins') as MarginsType;
    if (marginsType === MarginsType.NO_MARGINS) {
      return false;
    }

    if (marginsType === MarginsType.MINIMUM) {
      return true;
    }

    return !this.margins ||
        this.margins.get(CustomMarginsOrientation.TOP) > 0 ||
        this.margins.get(CustomMarginsOrientation.BOTTOM) > 0;
  }

  private updateRasterizeAvailable_() {
    // Need document settings to know if source is PDF.
    if (this.documentSettings === undefined) {
      return;
    }

    this.setSettingPath_('rasterize.available', this.isRasterizeAvailable_());
  }

  /**
   * @return Whether the rasterization setting should be available.
   */
  private isRasterizeAvailable_(): boolean {
    // Only a possibility for PDFs.  Always available for PDFs on Linux and
    // ChromeOS.  crbug.com/675798
    let available =
        !!this.documentSettings && !this.documentSettings.isModifiable;

    // <if expr="is_win or is_macosx">
    // Availability on Windows or macOS depends upon policy.
    if (!available || !this.policySettings_) {
      return false;
    }
    const policy = this.policySettings_['printPdfAsImageAvailability'];
    available = policy !== undefined && policy.value;
    // </if>

    return available;
  }

  private isLayoutAvailable_(caps: CddCapabilities|null): boolean {
    if (!caps || !caps.page_orientation || !caps.page_orientation.option ||
        (!this.documentSettings.isModifiable &&
         !this.documentSettings.isFromArc) ||
        this.documentSettings.allPagesHaveCustomOrientation) {
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
  }

  /**
   * @return Whether the borderless setting should be available.
   */
  private isBorderlessAvailable_(caps: CddCapabilities|null): boolean {
    return loadTimeData.getBoolean('isBorderlessPrintingEnabled') && !!caps &&
        !!caps.media_size?.option?.find(o => {
          return o.has_borderless_variant;
        });
  }

  private updateSettingsValues_(caps: CddCapabilities|null) {
    if (this.settings.mediaSize.available) {
      const defaultOption =
          caps!.media_size!.option.find(o => !!o.is_default) ||
          caps!.media_size!.option[0];
      let matchingOption = null;
      // If the setting does not have a valid value, the UI has just started so
      // do not try to get a matching value; just set the printer default in
      // case the user doesn't have sticky settings.
      if (this.settings.mediaSize.setFromUi) {
        const currentMediaSize = this.getSettingValue('mediaSize');
        matchingOption = caps!.media_size!.option.find(o => {
          return o.height_microns === currentMediaSize.height_microns &&
              o.width_microns === currentMediaSize.width_microns;
        });
      }
      this.setSetting('mediaSize', matchingOption || defaultOption, true);
    }

    if (this.settings.borderless.available) {
      this.setSetting(
          'borderless',
          this.settings.borderless.setFromUi &&
              this.getSettingValue('borderless'),
          true);
    }

    if (this.settings.mediaType.available) {
      const defaultOption =
          caps!.media_type!.option.find(o => !!o.is_default) ||
          caps!.media_type!.option[0];
      let matchingOption = null;
      if (this.settings.mediaType.setFromUi) {
        const currentMediaType = this.getSettingValue('mediaType');
        matchingOption = caps!.media_type!.option.find(o => {
          return o.vendor_id === currentMediaType.vendor_id;
        });
      }
      this.setSetting('mediaType', matchingOption || defaultOption, true);
    } else if (
        caps && caps.media_type && caps.media_type.option &&
        caps.media_type.option.length > 0) {
      const unavailableValue =
          caps!.media_type!.option.find(o => !!o.is_default) ||
          caps!.media_type!.option[0];
      this.setSettingPath_('mediaType.unavailableValue', unavailableValue);
    }

    if (this.settings.dpi.available) {
      const defaultOption =
          caps!.dpi!.option.find(o => !!o.is_default) || caps!.dpi!.option[0];
      let matchingOption = null;
      if (this.settings.dpi.setFromUi) {
        const currentDpi = this.getSettingValue('dpi');
        matchingOption = caps!.dpi!.option.find(o => {
          return o.horizontal_dpi === currentDpi.horizontal_dpi &&
              o.vertical_dpi === currentDpi.vertical_dpi;
        });
      }
      this.setSetting('dpi', matchingOption || defaultOption, true);
    } else if (
        caps && caps.dpi && caps.dpi.option && caps.dpi.option.length > 0) {
      const unavailableValue =
          caps!.dpi!.option.find(o => !!o.is_default) || caps!.dpi!.option[0];
      this.setSettingPath_('dpi.unavailableValue', unavailableValue);
    }

    if (!this.settings.color.setFromUi && this.settings.color.available) {
      const defaultOption = this.destination.defaultColorOption;
      if (defaultOption) {
        this.setSetting(
            'color',
            !['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'].includes(
                defaultOption.type!),
            true);
      }
    } else if (
        !this.settings.color.available && caps && caps.color &&
        caps.color.option && caps.color.option.length > 0) {
      this.setSettingPath_(
          'color.unavailableValue',
          !['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'].includes(
              caps.color.option[0].type!));
    } else if (!this.settings.color.available) {
      // if no color capability is reported, assume black and white.
      this.setSettingPath_('color.unavailableValue', false);
    }

    // Duplex policy is available on ChromeOS only. Therefore, we don't need to
    // check printing destinations' duplex availability on other platforms.
    // <if expr="is_chromeos">
    const duplexPolicyDefaultValueAvailable =
        getDuplexPolicyDefaultValueAvailable(
            this.getSetting('duplex').policyDefaultValue,
            this.getSetting('duplexShortEdge').policyDefaultValue, caps);
    // </if>
    // <if expr="not is_chromeos">
    const duplexPolicyDefaultValueAvailable = false;
    // </if>
    if (!this.settings.duplex.setFromUi && this.settings.duplex.available &&
        !duplexPolicyDefaultValueAvailable) {
      const defaultOption = caps!.duplex!.option.find(o => !!o.is_default);
      if (defaultOption !== undefined) {
        const defaultOptionIsDuplex =
            defaultOption.type === DuplexType.SHORT_EDGE ||
            defaultOption.type === DuplexType.LONG_EDGE;
        this.setSetting('duplex', defaultOptionIsDuplex, true);
        if (defaultOptionIsDuplex) {
          this.setSetting(
              'duplexShortEdge', defaultOption.type === DuplexType.SHORT_EDGE,
              true);
        }

        if (!this.settings.duplexShortEdge.available) {
          // Duplex is available, so must have only one two sided printing
          // option. Set duplexShortEdge's unavailable value based on the
          // printer.
          this.setSettingPath_(
              'duplexShortEdge.unavailableValue',
              caps!.duplex!.option.some(o => o.type === DuplexType.SHORT_EDGE));
        }
      }
    } else if (
        !this.settings.duplex.available && caps && caps.duplex &&
        caps.duplex.option) {
      // In this case, there must only be one option.
      const hasLongEdge =
          caps!.duplex!.option.some(o => o.type === DuplexType.LONG_EDGE);
      const hasShortEdge =
          caps!.duplex!.option.some(o => o.type === DuplexType.SHORT_EDGE);
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
      const vendorSettings: {[key: string]: any} = {};
      for (const item of caps!.vendor_capability!) {
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
  }

  /**
   * Caches the sticky settings and sets up the recent destinations. Sticky
   * settings will be applied when destinaton capabilities have been retrieved.
   */
  setStickySettings(savedSettingsStr: string|null) {
    assert(!this.stickySettings_);

    if (!savedSettingsStr) {
      return;
    }

    let savedSettings;
    try {
      savedSettings = JSON.parse(savedSettingsStr) as SerializedSettings;
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

    // Remove unsupported privet and cloud printers from the sticky settings,
    // to free up these spots for supported printers.
    const unsupportedOrigins: DestinationOrigin[] = [
      DestinationOrigin.COOKIES,
      // <if expr="is_chromeos">
      DestinationOrigin.DEVICE,
      // </if>
      DestinationOrigin.PRIVET,
    ];
    recentDestinations = recentDestinations.filter((d: RecentDestination) => {
      return !unsupportedOrigins.includes(d.origin);
    });

    // Initialize recent destinations early so that the destination store can
    // start trying to fetch them.
    this.setSetting('recentDestinations', recentDestinations);
    savedSettings.recentDestinations = recentDestinations;

    this.stickySettings_ = savedSettings;
  }

  /**
   * Helper function for configurePolicySetting_(). Sets value and managed flag
   * for given setting.
   * @param settingName Name of the setting being applied.
   * @param value Value of the setting provided via policy.
   * @param managed Flag showing whether value of setting is managed.
   * @param applyOnDestinationUpdate Flag showing whether policy
   *     should be applied on every destination update.
   */
  private setPolicySetting_(
      settingName: string, value: any, managed: boolean,
      applyOnDestinationUpdate: boolean) {
    if (!this.policySettings_) {
      this.policySettings_ = {};
    }
    (this.policySettings_ as {[key: string]: PolicyEntry})[settingName] = {
      value: value,
      managed: managed,
      applyOnDestinationUpdate: applyOnDestinationUpdate,
    };
  }

  /**
   * Helper function for setPolicySettings(). Calculates value and managed flag
   * of the setting according to allowed and default modes.
   */
  private configurePolicySetting_(
      settingName: string, allowedMode: any, defaultMode: any) {
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
      // <if expr="is_chromeos">
      case 'color': {
        const value = allowedMode ? allowedMode : defaultMode;
        if (value !== undefined) {
          this.setPolicySetting_(
              settingName, value, !!allowedMode,
              /*applyOnDestinationUpdate=*/ false);
        }
        break;
      }
      case 'duplex': {
        const value = getDuplexDefaultValue(allowedMode, defaultMode);
        if (value !== undefined) {
          this.setPolicySetting_(
              settingName, value, !!allowedMode,
              /*applyOnDestinationUpdate=*/ false);
        }
        break;
      }
      case 'pin': {
        const value = allowedMode ? allowedMode : defaultMode;
        if (value !== undefined) {
          this.setPolicySetting_(
              settingName, value, !!allowedMode,
              /*applyOnDestinationUpdate=*/ false);
        }
        break;
      }
      // </if>
      // <if expr="is_win or is_macosx">
      case 'printPdfAsImageAvailability': {
        const value = allowedMode !== undefined ? allowedMode : defaultMode;
        if (value !== undefined) {
          this.setPolicySetting_(
              settingName, value, /*managed=*/ false,
              /*applyOnDestinationUpdate=*/ false);
        }
        break;
      }
      // </if>
      case 'printPdfAsImage': {
        if (defaultMode !== undefined) {
          this.setPolicySetting_(
              settingName, defaultMode, /*managed=*/ false,
              /*applyOnDestinationUpdate=*/ false);
        }
        break;
      }
      default:
        break;
    }
  }

  /**
   * Sets settings in accordance to policies from native code, and prevents
   * those settings from being changed via other means.
   */
  setPolicySettings(policies: Policies|undefined) {
    if (policies === undefined) {
      return;
    }
    const policiesObject = policies as {[key: string]: PolicyObjectEntry};
    ['headerFooter', 'cssBackground', 'mediaSize'].forEach(settingName => {
      if (!policiesObject[settingName]) {
        return;
      }
      const defaultMode = policiesObject[settingName].defaultMode;
      const allowedMode = policiesObject[settingName].allowedMode;
      this.configurePolicySetting_(settingName, allowedMode, defaultMode);
    });
    // <if expr="is_chromeos">
    if (policiesObject['sheets']) {
      if (!this.policySettings_) {
        this.policySettings_ = {};
      }
      this.policySettings_['sheets'] = {
        value: policiesObject['sheets'].value,
        applyOnDestinationUpdate: false,
        managed: true,
      };
    }
    ['color', 'duplex', 'pin'].forEach(settingName => {
      if (!policiesObject[settingName]) {
        return;
      }
      const defaultMode = policiesObject[settingName].defaultMode;
      const allowedMode = policiesObject[settingName].allowedMode;
      this.configurePolicySetting_(settingName, allowedMode, defaultMode);
    });
    // </if>
    // <if expr="is_win or is_macosx">
    if (policies['printPdfAsImageAvailability']) {
      if (!this.policySettings_) {
        this.policySettings_ = {};
      }
      const allowedMode = policies['printPdfAsImageAvailability'].allowedMode;
      this.configurePolicySetting_(
          'printPdfAsImageAvailability', allowedMode, /*defaultMode=*/ false);
    }
    // </if>
    if (policies['printPdfAsImage']) {
      if (!this.policySettings_) {
        this.policySettings_ = {};
      }
      const defaultMode = policies['printPdfAsImage'].defaultMode;
      this.configurePolicySetting_(
          'printPdfAsImage', /*allowedMode=*/ undefined, defaultMode);
    }
  }

  applyStickySettings() {
    if (this.stickySettings_) {
      STICKY_SETTING_NAMES.forEach(settingName => {
        const setting = this.get(settingName, this.settings) as Setting;
        const value =
            (this.stickySettings_ as {[key: string]: any})[setting.key];
        if (value !== undefined) {
          this.setSetting(settingName, value);
        } else {
          this.applyScalingStickySettings_(settingName);
        }
      });
    }
    this.applyPersistentCddDefaults_();
    this.applyPolicySettings_();
    this.initialized_ = true;
    this.updateManaged_();
    this.stickySettings_ = null;
    this.fire_('sticky-setting-changed', this.getStickySettings_());
  }

  /**
   * Helper function for applyStickySettings(). Checks if the setting
   * is a scaling setting and applies by applying the old types
   * that rely on 'fitToPage' and 'customScaling'.
   * @param settingName Name of the setting being applied.
   */
  private applyScalingStickySettings_(settingName: string) {
    // TODO(dhoss): Remove checks for 'customScaling' and 'fitToPage'
    if (settingName === 'scalingType' &&
        'customScaling' in this.stickySettings_!) {
      const isCustom = this.stickySettings_['customScaling'];
      const scalingType = isCustom ? ScalingType.CUSTOM : ScalingType.DEFAULT;
      this.setSetting(settingName, scalingType);
    } else if (settingName === 'scalingTypePdf') {
      if ('isFitToPageEnabled' in this.stickySettings_!) {
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
  }

  private applyPolicySettings_() {
    if (this.policySettings_) {
      for (const [settingName, policy] of Object.entries(
               this.policySettings_)) {
        const policyEntry = policy as PolicyEntry;
        // <if expr="is_chromeos">
        if (settingName === 'sheets') {
          this.maxSheets = policyEntry.value;
          continue;
        }
        if (settingName === 'color') {
          this.set(
              'settings.color.value',
              policyEntry.value === ColorModeRestriction.COLOR);
          this.set('settings.color.setByPolicy', policyEntry.managed);
          continue;
        }
        if (settingName === 'duplex') {
          const isDuplex =
              (policyEntry.value === DuplexModeRestriction.SHORT_EDGE ||
               policyEntry.value === DuplexModeRestriction.LONG_EDGE ||
               policyEntry.value === DuplexModeRestriction.DUPLEX);

          this.set('settings.duplex.value', isDuplex);
          this.set('settings.duplex.policyDefaultValue', isDuplex);

          if (policyEntry.value === DuplexModeRestriction.SHORT_EDGE ||
              policyEntry.value === DuplexModeRestriction.LONG_EDGE) {
            this.set(
                'settings.duplexShortEdge.value',
                policyEntry.value === DuplexModeRestriction.SHORT_EDGE);
            this.set(
                'settings.duplexShortEdge.policyDefaultValue',
                policyEntry.value === DuplexModeRestriction.SHORT_EDGE);
          }

          this.set('settings.duplex.setByPolicy', policyEntry.managed);
          // Duplex mode is never set by policy
          this.set('settings.duplexShortEdge.setByPolicy', false);
          continue;
        }
        if (settingName === 'pin') {
          if (policyEntry.value === PinModeRestriction.NO_PIN &&
              policyEntry.managed) {
            this.set('settings.pin.available', false);
            this.set('settings.pinValue.available', false);
          } else {
            this.set(
                'settings.pin.value',
                policyEntry.value === PinModeRestriction.PIN);
          }
          this.set('settings.pin.setByPolicy', policyEntry.managed);
          continue;
        }
        // </if>
        // <if expr="is_win or is_macosx">
        if (settingName === 'printPdfAsImageAvailability') {
          this.updateRasterizeAvailable_();
          if (this.settings.rasterize.available) {
            // If rasterize is available then otherOptions must be available.
            this.setSettingPath_('otherOptions.available', true);
          }
          continue;
        }
        // </if>
        if (settingName === 'printPdfAsImage') {
          if (policyEntry.value) {
            this.setSetting('rasterize', policyEntry.value, true);
          }
          continue;
        }
        if (policyEntry.value !== undefined &&
            !policyEntry.applyOnDestinationUpdate) {
          this.setSetting(
              settingName as keyof Settings, policyEntry.value, true);
          if (policyEntry.managed) {
            this.set(`settings.${settingName}.setByPolicy`, true);
          }
        }
      }
    }
  }

  /**
   * If the setting has a default value specified in the CDD capabilities and
   * the attribute `reset_to_default` is true, this method will return the
   * default value for the setting; otherwise it will return null.
   */
  private getResetValue_(capability: CapabilityWithReset): (object|null) {
    if (!capability.reset_to_default) {
      return null;
    }
    const cddDefault = capability.option.find(o => !!o.is_default);
    if (!cddDefault) {
      return null;
    }
    return cddDefault;
  }

  /**
   * For PrinterProvider printers, it's possible to specify for a setting to
   * always reset to the default value using the `reset_to_default` attribute.
   * If `reset_to_default` is true and a default value for the
   * setting is specified, this method will reset the setting
   * value to the default value.
   */
  private applyPersistentCddDefaults_() {
    if (!this.destination || !this.destination.isExtension) {
      return;
    }

    const caps = this.destination && this.destination.capabilities ?
        this.destination.capabilities.printer :
        null;
    if (!caps) {
      return;
    }

    if (this.settings.mediaSize.available) {
      const cddDefault = this.getResetValue_(caps['media_size']!);
      if (cddDefault) {
        this.set('settings.mediaSize.value', cddDefault);
      }
    }

    if (this.settings.mediaType.available) {
      assert(loadTimeData.getBoolean('isBorderlessPrintingEnabled'));
      const cddDefault = this.getResetValue_(caps['media_type']!);
      if (cddDefault) {
        this.set('settings.mediaType.value', cddDefault);
      }
    }

    if (this.settings.color.available) {
      const cddDefault = this.getResetValue_(caps['color']!) as ColorOption;
      if (cddDefault) {
        this.set(
            'settings.color.value',
            !['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'].includes(
                cddDefault.type!));
      }
    }

    if (this.settings.duplex.available) {
      const cddDefault = this.getResetValue_(caps['duplex']!) as DuplexOption;
      if (cddDefault) {
        this.set(
            'settings.duplex.value',
            cddDefault.type === DuplexType.LONG_EDGE ||
                cddDefault.type === DuplexType.SHORT_EDGE);
        if (!this.settings.duplexShortEdge.available) {
          this.set(
              'settings.duplexShortEdge.value',
              cddDefault.type === DuplexType.SHORT_EDGE);
        }
      }
    }

    if (this.settings.dpi.available) {
      const cddDefault = this.getResetValue_(caps['dpi']!);
      if (cddDefault) {
        this.set('settings.dpi.value', cddDefault);
      }
    }
  }

  /**
   * Restricts settings and applies defaults as defined by policy applicable to
   * current destination.
   */
  applyDestinationSpecificPolicies() {
    if (this.settings.mediaSize.available && this.policySettings_) {
      const mediaSizePolicy = this.policySettings_['mediaSize'] &&
          this.policySettings_['mediaSize'].value;
      if (mediaSizePolicy !== undefined) {
        const matchingOption =
            this.destination.capabilities!.printer.media_size!.option.find(
                o => {
                  return o.width_microns === mediaSizePolicy.width &&
                      o.height_microns === mediaSizePolicy.height;
                });
        if (matchingOption !== undefined) {
          this.set('settings.mediaSize.value', matchingOption);
        }
      }
    }

    this.updateManaged_();
  }

  private updateManaged_() {
    let managedSettings: Array<keyof Settings> =
        ['cssBackground', 'headerFooter'];
    // <if expr="is_chromeos">
    managedSettings =
        managedSettings.concat(['color', 'duplex', 'duplexShortEdge', 'pin']);
    // </if>
    this.settingsManaged = managedSettings.some(settingName => {
      const setting = this.getSetting(settingName);
      return setting.available && setting.setByPolicy;
    });
  }

  initialized(): boolean {
    return this.initialized_;
  }

  private getStickySettings_(): string {
    const serialization: {[key: string]: any} = {};
    serialization['version'] = 2;

    STICKY_SETTING_NAMES.forEach(settingName => {
      const setting = this.get(settingName, this.settings);
      if (setting.setFromUi) {
        serialization[setting.key] = setting.value;
      }
    });

    return JSON.stringify(serialization);
  }

  private getDuplexMode_(): DuplexMode {
    if (!this.getSettingValue('duplex')) {
      return DuplexMode.SIMPLEX;
    }

    return this.getSettingValue('duplexShortEdge') ? DuplexMode.SHORT_EDGE :
                                                     DuplexMode.LONG_EDGE;
  }

  private getCddDuplexType_(): DuplexType {
    if (!this.getSettingValue('duplex')) {
      return DuplexType.NO_DUPLEX;
    }

    return this.getSettingValue('duplexShortEdge') ? DuplexType.SHORT_EDGE :
                                                     DuplexType.LONG_EDGE;
  }

  /**
   * Creates a string that represents a print ticket.
   * @param destination Destination to print to.
   * @param openPdfInPreview Whether this print request is to open
   *     the PDF in Preview app (Mac only).
   * @param showSystemDialog Whether this print request is to show
   *     the system dialog.
   * @return Serialized print ticket.
   */
  createPrintTicket(
      destination: Destination, openPdfInPreview: boolean,
      showSystemDialog: boolean): string {
    const dpi = this.getSettingValue('dpi') as DpiOption;
    const scalingSettingKey = this.getSetting('scalingTypePdf').available ?
        'scalingTypePdf' :
        'scalingType';
    const ticket: PrintTicket = {
      mediaSize: this.getSettingValue('mediaSize') as MediaSizeValue,
      borderless: loadTimeData.getBoolean('isBorderlessPrintingEnabled') &&
          this.getSettingValue('mediaSize')?.has_borderless_variant &&
          this.getSettingValue('borderless'),
      mediaType: this.getSettingValue('mediaType')?.vendor_id,
      pageCount: this.getSettingValue('pages').length,
      landscape: this.getSettingValue('layout'),
      color: destination.getNativeColorModel(
          this.getSettingValue('color') as boolean),
      headerFooterEnabled: false,  // only used in print preview
      marginsType: this.getSettingValue('margins'),
      duplex: this.getDuplexMode_(),
      copies: this.getSettingValue('copies'),
      collate: this.getSettingValue('collate'),
      shouldPrintBackgrounds: this.getSettingValue('cssBackground'),
      shouldPrintSelectionOnly: false,  // only used in print preview
      previewModifiable: this.documentSettings.isModifiable,
      printerType: destination.type,
      rasterizePDF: this.getSettingValue('rasterize'),
      scaleFactor:
          this.getSettingValue(scalingSettingKey) === ScalingType.CUSTOM ?
          parseInt(this.getSettingValue('scaling'), 10) :
          100,
      scalingType: this.getSettingValue(scalingSettingKey),
      pagesPerSheet: this.getSettingValue('pagesPerSheet'),
      dpiHorizontal: (dpi && 'horizontal_dpi' in dpi) ? dpi.horizontal_dpi : 0,
      dpiVertical: (dpi && 'vertical_dpi' in dpi) ? dpi.vertical_dpi : 0,
      dpiDefault: (dpi && 'is_default' in dpi) ? dpi.is_default! : false,
      deviceName: destination.id,
      pageWidth: this.pageSize.width,
      pageHeight: this.pageSize.height,
      showSystemDialog: showSystemDialog,
      // <if expr="is_chromeos">
      printToGoogleDrive:
          destination.id === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS,
      printerManuallySelected: destination.printerManuallySelected,
      // </if>
    };

    if (openPdfInPreview) {
      ticket['openPDFInPreview'] = openPdfInPreview;
    }

    if (this.getSettingValue('margins') === MarginsType.CUSTOM) {
      ticket['marginsCustom'] = this.getSettingValue('customMargins');
    }

    if (destination.isExtension) {
      // TODO(rbpotter): Get local and PDF printers to use the same ticket and
      // send only this ticket instead of nesting it in a larger ticket.
      ticket['ticket'] = this.createCloudJobTicket(destination);
      ticket['capabilities'] = JSON.stringify(destination.capabilities);
    }

    // <if expr="is_chromeos">
    if (this.getSettingValue('pin')) {
      ticket['pinValue'] = this.getSettingValue('pinValue');
    }
    if (destination.origin === DestinationOrigin.CROS) {
      ticket['advancedSettings'] = this.getSettingValue('vendorItems');
      ticket['printerStatusReason'] =
          destination.printerStatusReason || PrinterStatusReason.UNKNOWN_REASON;
    }
    // </if>

    return JSON.stringify(ticket);
  }

  /**
   * Creates an object that represents a Google Cloud Print print ticket.
   * @param destination Destination to print to.
   * @return Google Cloud Print print ticket.
   */
  createCloudJobTicket(destination: Destination): string {
    assert(
        destination.isExtension,
        'Trying to create a Google Cloud Print print ticket for a local ' +
            ' non-extension destination');
    assert(
        destination.capabilities,
        'Trying to create a Google Cloud Print print ticket for a ' +
            'destination with no print capabilities');

    // Create CJT (Cloud Job Ticket)
    const cjt: CloudJobTicket = {version: '1.0', print: {}};
    if (this.settings.collate.available) {
      cjt.print.collate = {collate: this.settings.collate.value};
    }
    if (this.settings.color.available) {
      const selectedOption = destination.getSelectedColorOption(
          this.settings.color.value as boolean);
      if (!selectedOption) {
        console.warn('Could not find correct color option');
      } else {
        cjt.print.color = {type: selectedOption.type};
        if (selectedOption.hasOwnProperty('vendor_id')) {
          cjt.print.color!.vendor_id = selectedOption.vendor_id;
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
          cjt.print.color!.vendor_id = defaultOption.vendor_id;
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
        vendor_id: mediaValue.vendor_id,
      };
    }
    if (!this.settings.layout.available) {
      // In this case "orientation" option is hidden from user, so user can't
      // adjust it for page content, see Landscape.isCapabilityAvailable().
      // We can improve results if we set AUTO here.
      const capability = destination.capabilities!.printer ?
          destination.capabilities!.printer.page_orientation :
          null;
      if (capability && capability.option &&
          capability.option.some(option => option.type === 'AUTO')) {
        cjt.print.page_orientation = {type: 'AUTO'};
      }
    } else {
      cjt.print.page_orientation = {
        type: this.settings.layout.value ? 'LANDSCAPE' : 'PORTRAIT',
      };
    }
    if (this.settings.dpi.available) {
      const dpiValue = this.settings.dpi.value;
      cjt.print.dpi = {
        horizontal_dpi: dpiValue.horizontal_dpi,
        vertical_dpi: dpiValue.vertical_dpi,
        vendor_id: dpiValue.vendor_id,
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
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-model': PrintPreviewModelElement;
  }
}

customElements.define(PrintPreviewModelElement.is, PrintPreviewModelElement);
