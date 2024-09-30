// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';

// <if expr="is_chromeos">
import {NativeLayerCrosImpl} from '../native_layer_cros.js';

// </if>

import type {Cdd, ColorCapability, ColorOption, CopiesCapability} from './cdd.js';

// <if expr="is_chromeos">
import type {PrinterStatus} from './printer_status_cros.js';
import {getStatusReasonFromPrinterStatus, PrinterStatusReason} from './printer_status_cros.js';
// </if>

/**
 * Enumeration of the origin types for destinations.
 */
export enum DestinationOrigin {
  LOCAL = 'local',
  // Note: Cookies, device and privet are deprecated, but used to filter any
  // legacy entries in the recent destinations, since we can't guarantee all
  // such recent printers have been overridden.
  COOKIES = 'cookies',
  // <if expr="is_chromeos">
  DEVICE = 'device',
  // </if>
  PRIVET = 'privet',
  EXTENSION = 'extension',
  CROS = 'chrome_os',
}

/**
 * Printer types for capabilities and printer list requests.
 * Must match PrinterType in printing/mojom/print.mojom
 */
export enum PrinterType {
  PRIVET_PRINTER_DEPRECATED = 0,
  EXTENSION_PRINTER = 1,
  PDF_PRINTER = 2,
  LOCAL_PRINTER = 3,
  CLOUD_PRINTER_DEPRECATED = 4
}

// <if expr="is_chromeos">
/**
 * Enumeration specifying whether a destination is provisional and the reason
 * the destination is provisional.
 */
export enum DestinationProvisionalType {
  // Destination is not provisional.
  NONE = 'NONE',
  // User has to grant USB access for the destination to its provider.
  // Used for destinations with extension origin.
  NEEDS_USB_PERMISSION = 'NEEDS_USB_PERMISSION',
}
// </if>

/**
 * Enumeration of color modes used by Chromium.
 */
export enum ColorMode {
  GRAY = 1,
  COLOR = 2,
}

export interface RecentDestination {
  id: string;
  origin: DestinationOrigin;
  capabilities: Cdd|null;
  displayName: string;
  extensionId: string;
  extensionName: string;
  icon?: string;
}

export function isPdfPrinter(id: string): boolean {
  // <if expr="is_chromeos">
  if (id === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
    return true;
  }
  // </if>

  return id === GooglePromotedDestinationId.SAVE_AS_PDF;
}

/**
 * Creates a |RecentDestination| to represent |destination| in the app
 * state.
 */
export function makeRecentDestination(destination: Destination):
    RecentDestination {
  return {
    id: destination.id,
    origin: destination.origin,
    capabilities: destination.capabilities,
    displayName: destination.displayName || '',
    extensionId: destination.extensionId || '',
    extensionName: destination.extensionName || '',
    icon: destination.icon || '',
  };
}

/**
 * @return key that maps to a destination with the selected |id| and |origin|.
 */
export function createDestinationKey(
    id: string, origin: DestinationOrigin): string {
  return `${id}/${origin}/`;
}

/**
 * @return A key that maps to a destination with parameters matching
 *     |recentDestination|.
 */
export function createRecentDestinationKey(
    recentDestination: RecentDestination): string {
  return createDestinationKey(recentDestination.id, recentDestination.origin);
}

export interface DestinationOptionalParams {
  isEnterprisePrinter?: boolean;
  // <if expr="is_chromeos">
  provisionalType?: DestinationProvisionalType;
  // </if>
  extensionId?: string;
  extensionName?: string;
  description?: string;
  location?: string;
}

/**
 * List of capability types considered color.
 */
const COLOR_TYPES: string[] = ['STANDARD_COLOR', 'CUSTOM_COLOR'];

/**
 * List of capability types considered monochrome.
 */
const MONOCHROME_TYPES: string[] = ['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'];


/**
 * Print destination data object.
 */
export class Destination {
  /**
   * ID of the destination.
   */
  private id_: string;

  /**
   * Origin of the destination.
   */
  private origin_: DestinationOrigin;

  /**
   * Display name of the destination.
   */
  private displayName_: string;

  /**
   * Print capabilities of the destination.
   */
  private capabilities_: Cdd|null = null;

  /**
   * Whether the destination is an enterprise policy controlled printer.
   */
  private isEnterprisePrinter_: boolean;

  /**
   * Destination location.
   */
  private location_: string = '';

  /**
   * Printer description.
   */
  private description_: string;

  /**
   * Extension ID for extension managed printers.
   */
  private extensionId_: string;

  /**
   * Extension name for extension managed printers.
   */
  private extensionName_: string;

  // <if expr="is_chromeos">
  /**
   * Different from  DestinationProvisionalType.NONE if
   * the destination is provisional. Provisional destinations cannot be
   * selected as they are, but have to be resolved first (i.e. extra steps
   * have to be taken to get actual destination properties, which should
   * replace the provisional ones). Provisional destination resolvment flow
   * will be started when the user attempts to select the destination in
   * search UI.
   */
  private provisionalType_: DestinationProvisionalType;

  /**
   * EULA url for printer's PPD. Empty string indicates no provided EULA.
   */
  private eulaUrl_: string = '';

  /**
   * True if the user opened the print preview dropdown and selected a different
   * printer than the original destination.
   */
  private printerManuallySelected_: boolean = false;

  /**
   * Stores the printer status reason for a local Chrome OS printer.
   */
  private printerStatusReason_: PrinterStatusReason|null = null;

  /**
   * Promise returns |key_| when the printer status request is completed.
   */
  private printerStatusRequestedPromise_: Promise<string>|null = null;

  /**
   * True if the failed printer status request has already been retried once.
   */
  private printerStatusRetrySent_: boolean = false;

  /**
   * The length of time to wait before retrying a printer status request.
   */
  private printerStatusRetryTimerMs_: number = 3000;
  // </if>

  private type_: PrinterType;

  constructor(
      id: string, origin: DestinationOrigin, displayName: string,
      params?: DestinationOptionalParams) {
    this.id_ = id;
    this.origin_ = origin;
    this.displayName_ = displayName || '';
    this.isEnterprisePrinter_ = (params && params.isEnterprisePrinter) || false;
    this.description_ = (params && params.description) || '';
    this.extensionId_ = (params && params.extensionId) || '';
    this.extensionName_ = (params && params.extensionName) || '';
    this.location_ = (params && params.location) || '';
    this.type_ = this.computeType_(id, origin);
    // <if expr="is_chromeos">
    this.provisionalType_ =
        (params && params.provisionalType) || DestinationProvisionalType.NONE;

    assert(
        this.provisionalType_ !==
                DestinationProvisionalType.NEEDS_USB_PERMISSION ||
            this.isExtension,
        'Provisional USB destination only supprted with extension origin.');
    // </if>
  }

  private computeType_(id: string, origin: DestinationOrigin): PrinterType {
    if (isPdfPrinter(id)) {
      return PrinterType.PDF_PRINTER;
    }

    return origin === DestinationOrigin.EXTENSION ?
        PrinterType.EXTENSION_PRINTER :
        PrinterType.LOCAL_PRINTER;
  }

  get type(): PrinterType {
    return this.type_;
  }

  get id(): string {
    return this.id_;
  }

  get origin(): DestinationOrigin {
    return this.origin_;
  }

  get displayName(): string {
    return this.displayName_;
  }

  /**
   * @return Whether the destination is an extension managed printer.
   */
  get isExtension(): boolean {
    return this.origin_ === DestinationOrigin.EXTENSION;
  }

  /**
   * @return Most relevant string to help user to identify this
   *     destination.
   */
  get hint(): string {
    return this.location_ || this.extensionName || this.description_;
  }

  /**
   * @return Extension ID associated with the destination. Non-empty
   *     only for extension managed printers.
   */
  get extensionId(): string {
    return this.extensionId_;
  }

  /**
   * @return Extension name associated with the destination.
   *     Non-empty only for extension managed printers.
   */
  get extensionName(): string {
    return this.extensionName_;
  }

  /** @return Print capabilities of the destination. */
  get capabilities(): Cdd|null {
    return this.capabilities_;
  }

  set capabilities(capabilities: Cdd|null) {
    if (capabilities) {
      this.capabilities_ = capabilities;
    }
  }

  // <if expr="is_chromeos">
  get eulaUrl(): string {
    return this.eulaUrl_;
  }

  set eulaUrl(eulaUrl: string) {
    this.eulaUrl_ = eulaUrl;
  }

  get printerManuallySelected(): boolean {
    return this.printerManuallySelected_;
  }

  set printerManuallySelected(printerManuallySelected: boolean) {
    this.printerManuallySelected_ = printerManuallySelected;
  }

  /**
   * @return The printer status reason for a local Chrome OS printer.
   */
  get printerStatusReason(): PrinterStatusReason|null {
    return this.printerStatusReason_;
  }

  set printerStatusReason(printerStatusReason: PrinterStatusReason) {
    this.printerStatusReason_ = printerStatusReason;
  }

  setPrinterStatusRetryTimeoutForTesting(timeoutMs: number) {
    this.printerStatusRetryTimerMs_ = timeoutMs;
  }

  /**
   * Requests a printer status for the destination.
   * @return Promise with destination key.
   */
  requestPrinterStatus(): Promise<string> {
    // Requesting printer status only allowed for local CrOS printers.
    if (this.origin_ !== DestinationOrigin.CROS) {
      return Promise.reject();
    }

    // Immediately resolve promise if |printerStatusReason_| is already
    // available.
    if (this.printerStatusReason_) {
      return Promise.resolve(this.key);
    }

    // Return existing promise if the printer status has already been requested.
    if (this.printerStatusRequestedPromise_) {
      return this.printerStatusRequestedPromise_;
    }

    // Request printer status then set and return the promise.
    this.printerStatusRequestedPromise_ = this.requestPrinterStatusPromise_();
    return this.printerStatusRequestedPromise_;
  }

  /**
   * Requests a printer status for the destination. If the printer status comes
   * back as |PRINTER_UNREACHABLE|, this function will retry and call itself
   * again once before resolving the original call.
   * @return Promise with destination key.
   */
  private requestPrinterStatusPromise_(): Promise<string> {
    return NativeLayerCrosImpl.getInstance()
        .requestPrinterStatusUpdate(this.id_)
        .then(status => {
          if (status) {
            const statusReason =
                getStatusReasonFromPrinterStatus(status as PrinterStatus);
            const isPrinterUnreachable =
                statusReason === PrinterStatusReason.PRINTER_UNREACHABLE;
            if (isPrinterUnreachable && !this.printerStatusRetrySent_) {
              this.printerStatusRetrySent_ = true;
              return this.printerStatusWaitForTimerPromise_();
            }

            this.printerStatusReason_ = statusReason;
          }
          return Promise.resolve(this.key);
        });
  }

  /**
   * Pause for a set timeout then retry the printer status request.
   * @return Promise with destination key.
   */
  private printerStatusWaitForTimerPromise_(): Promise<string> {
    return new Promise<void>((resolve, _reject) => {
             setTimeout(() => {
               resolve();
             }, this.printerStatusRetryTimerMs_);
           })
        .then(() => {
          return this.requestPrinterStatusPromise_();
        });
  }

  /** @return Whether the destination is ready to be selected. */
  get readyForSelection(): boolean {
    return (this.origin_ !== DestinationOrigin.CROS ||
            this.capabilities_ !== null) &&
        !this.isProvisional;
  }

  get provisionalType(): DestinationProvisionalType {
    return this.provisionalType_;
  }

  get isProvisional(): boolean {
    return this.provisionalType_ !== DestinationProvisionalType.NONE;
  }
  // </if>

  /** @return Path to the SVG for the destination's icon. */
  get icon(): string {
    // <if expr="is_chromeos">
    if (this.id_ === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return 'print-preview:save-to-drive';
    }
    // </if>
    if (this.id_ === GooglePromotedDestinationId.SAVE_AS_PDF) {
      return 'cr:insert-drive-file';
    }
    if (this.isEnterprisePrinter) {
      return 'print-preview:business';
    }
    return 'print-preview:print';
  }

  /**
   * @return Properties (besides display name) to match search queries against.
   */
  get extraPropertiesToMatch(): string[] {
    return [this.location_, this.description_];
  }

  /**
   * Matches a query against the destination.
   * @param query Query to match against the destination.
   * @return Whether the query matches this destination.
   */
  matches(query: RegExp): boolean {
    return !!this.displayName_.match(query) ||
        !!this.extensionName_.match(query) || !!this.location_.match(query) ||
        !!this.description_.match(query);
  }

  /**
   * Whether the printer is enterprise policy controlled printer.
   */
  get isEnterprisePrinter(): boolean {
    return this.isEnterprisePrinter_;
  }

  private copiesCapability_(): CopiesCapability|null {
    return this.capabilities && this.capabilities.printer &&
            this.capabilities.printer.copies ?
        this.capabilities.printer.copies :
        null;
  }

  private colorCapability_(): ColorCapability|null {
    return this.capabilities && this.capabilities.printer &&
            this.capabilities.printer.color ?
        this.capabilities.printer.color :
        null;
  }

  /** @return Whether the printer supports copies. */
  get hasCopiesCapability(): boolean {
    const capability = this.copiesCapability_();
    if (!capability) {
      return false;
    }
    return capability.max ? capability.max > 1 : true;
  }

  /**
   * @return Whether the printer supports both black and white and
   *     color printing.
   */
  get hasColorCapability(): boolean {
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return false;
    }
    let hasColor = false;
    let hasMonochrome = false;
    capability.option.forEach(option => {
      assert(option.type);
      hasColor = hasColor || COLOR_TYPES.includes(option.type);
      hasMonochrome = hasMonochrome || MONOCHROME_TYPES.includes(option.type);
    });
    return hasColor && hasMonochrome;
  }

  /**
   * @param isColor Whether to use a color printing mode.
   * @return Selected color option.
   */
  getSelectedColorOption(isColor: boolean): ColorOption|null {
    const typesToLookFor = isColor ? COLOR_TYPES : MONOCHROME_TYPES;
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return null;
    }
    for (let i = 0; i < typesToLookFor.length; i++) {
      const matchingOptions = capability.option.filter(option => {
        return option.type === typesToLookFor[i];
      });
      if (matchingOptions.length > 0) {
        return matchingOptions[0];
      }
    }
    return null;
  }

  /**
   * @param isColor Whether to use a color printing mode.
   * @return Native color model of the destination.
   */
  getNativeColorModel(isColor: boolean): number {
    // For printers without capability, native color model is ignored.
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return isColor ? ColorMode.COLOR : ColorMode.GRAY;
    }
    const selected = this.getSelectedColorOption(isColor);
    const mode = parseInt(selected ? selected.vendor_id! : '', 10);
    if (isNaN(mode)) {
      return isColor ? ColorMode.COLOR : ColorMode.GRAY;
    }
    return mode;
  }

  /**
   * @return The default color option for the destination.
   */
  get defaultColorOption(): ColorOption|null {
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return null;
    }
    const defaultOptions = capability.option.filter(option => {
      return option.is_default;
    });
    return defaultOptions.length !== 0 ? defaultOptions[0] : null;
  }

  /** @return A unique identifier for this destination. */
  get key(): string {
    return `${this.id_}/${this.origin_}/`;
  }
}

/**
 * Enumeration of Google-promoted destination IDs.
 * @enum {string}
 */
export enum GooglePromotedDestinationId {
  SAVE_AS_PDF = 'Save as PDF',
  // <if expr="is_chromeos">
  SAVE_TO_DRIVE_CROS = 'Save to Drive CrOS',
  // </if>
}

/* Unique identifier for the Save as PDF destination */
export const PDF_DESTINATION_KEY: string =
    `${GooglePromotedDestinationId.SAVE_AS_PDF}/${DestinationOrigin.LOCAL}/`;

// <if expr="is_chromeos">
/* Unique identifier for the Save to Drive CrOS destination */
export const SAVE_TO_DRIVE_CROS_DESTINATION_KEY: string =
    `${GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS}/${
        DestinationOrigin.LOCAL}/`;
// </if>
