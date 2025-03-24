// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';

import {NativeLayerCrosImpl} from '../native_layer_cros.js';

import type {Cdd, ColorCapability, ColorOption, CopiesCapability, DpiOption, DuplexType, MediaSizeOption, MediaTypeOption} from './cdd.js';
import type {ManagedPrintOptions} from './managed_print_options_cros.js';
import {IPP_PRINT_QUALITY, managedPrintOptionsDuplexToCdd, managedPrintOptionsQualityToIpp} from './managed_print_options_cros.js';
import {getStatusReasonFromPrinterStatus, PrinterStatusReason} from './printer_status_cros.js';

/**
 * Enumeration of the origin types for destinations.
 */
export enum DestinationOrigin {
  LOCAL = 'local',
  // Note: Cookies, device and privet are deprecated, but used to filter any
  // legacy entries in the recent destinations, since we can't guarantee all
  // such recent printers have been overridden.
  COOKIES = 'cookies',
  DEVICE = 'device',
  PRIVET = 'privet',
  EXTENSION = 'extension',
  CROS = 'chrome_os',
}

/**
 * Printer types for capabilities and printer list requests.
 * Must match PrinterType in printing/mojom/print.mojom
 */
export enum PrinterType {
  EXTENSION_PRINTER = 0,
  PDF_PRINTER = 1,
  LOCAL_PRINTER = 2,
}

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
  if (id === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
    return true;
  }

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
  provisionalType?: DestinationProvisionalType;
  managedPrintOptions?: ManagedPrintOptions;
  extensionId?: string;
  extensionName?: string;
  description?: string;
  location?: string;
}

/**
 * For each setting the corresponding property of this interface is set to true
 * if all the following holds:
 *   - Allowed managed print options are present for this setting.
 *   - There's a non-empty intersection between values supported by printer and
 *     allowed values in managed print options.
 *   - There's at least a single value that the printer supports, but the policy
 *     for this printer disallows.
 * Otherwise the property equals to false.
 */
export interface AllowedManagedPrintOptionsApplied {
  mediaSize: boolean;
  mediaType: boolean;
  duplex: boolean;
  color: boolean;
  dpi: boolean;
  quality: boolean;
  printAsImage: boolean;
}

function createDefaultAllowedManagedPrintOptionsApplied() {
  return {
    mediaSize: false,
    mediaType: false,
    duplex: false,
    color: false,
    dpi: false,
    quality: false,
    printAsImage: false,
  };
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

  /**
   * Default/allowed values of print options for the given printer set by
   * policy.
   */
  private managedPrintOptions_: ManagedPrintOptions|null = null;

  /**
   * For each setting tracks whether the set of allowed values was set by the
   * destination specific policy.
   */
  allowedManagedPrintOptionsApplied: AllowedManagedPrintOptionsApplied;

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
    this.provisionalType_ =
        (params && params.provisionalType) || DestinationProvisionalType.NONE;

    assert(
        this.provisionalType_ !==
                DestinationProvisionalType.NEEDS_USB_PERMISSION ||
            this.isExtension,
        'Provisional USB destination only supported with extension origin.');

    this.managedPrintOptions_ = (params && params.managedPrintOptions) || null;
    this.allowedManagedPrintOptionsApplied =
        createDefaultAllowedManagedPrintOptionsApplied();
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
            const statusReason = getStatusReasonFromPrinterStatus(status);
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

  /**
   * @return Printer specific print job options set via policy.
   */
  get managedPrintOptions(): ManagedPrintOptions|null {
    return this.managedPrintOptions_;
  }

  /**
   * True if the managed print options applied restrictions on any setting.
   */
  allowedManagedPrintOptionsAppliedForAnySetting(): boolean {
    return this.allowedManagedPrintOptionsApplied.mediaSize ||
        this.allowedManagedPrintOptionsApplied.mediaType ||
        this.allowedManagedPrintOptionsApplied.duplex ||
        this.allowedManagedPrintOptionsApplied.color ||
        this.allowedManagedPrintOptionsApplied.dpi ||
        this.allowedManagedPrintOptionsApplied.quality ||
        this.allowedManagedPrintOptionsApplied.printAsImage;
  }

  /**
   * Merges capabilities with the managed print options.
   */
  applyAllowedManagedPrintOptions() {
    // Reset all the properties of `allowedManagedPrintOptionsApplied` to false.
    this.allowedManagedPrintOptionsApplied =
        createDefaultAllowedManagedPrintOptionsApplied();

    if (!this.managedPrintOptions_) {
      return;
    }

    assert(this.capabilities_);
    if (this.capabilities_.printer.media_size &&
        this.managedPrintOptions_.mediaSize?.allowedValues &&
        this.managedPrintOptions_.mediaSize.allowedValues.length > 0) {
      const allowedMediaSizeValues =
          this.managedPrintOptions_.mediaSize.allowedValues;

      const allowedSupportedValues =
          this.capabilities_.printer.media_size.option.filter(
              (supportedValue) => {
                return allowedMediaSizeValues.some((allowedValue) => {
                  return supportedValue.width_microns === allowedValue.width &&
                      supportedValue.height_microns === allowedValue.height;
                });
              });

      if (allowedSupportedValues.length > 0 &&
          allowedSupportedValues.length <
              this.capabilities_.printer.media_size.option.length) {
        this.capabilities_.printer.media_size.option = allowedSupportedValues;
        this.allowedManagedPrintOptionsApplied.mediaSize = true;
      }
    }

    if (this.capabilities_.printer.media_type &&
        this.managedPrintOptions_.mediaType?.allowedValues &&
        this.managedPrintOptions_.mediaType.allowedValues.length > 0) {
      const allowedMediaTypeValues =
          this.managedPrintOptions_.mediaType.allowedValues;

      const allowedSupportedValues =
          this.capabilities_.printer.media_type.option.filter(
              (supportedValue) => {
                return allowedMediaTypeValues.some((allowedValue) => {
                  return supportedValue.vendor_id === allowedValue;
                });
              });

      if (allowedSupportedValues.length > 0 &&
          allowedSupportedValues.length <
              this.capabilities_.printer.media_type.option.length) {
        this.capabilities_.printer.media_type.option = allowedSupportedValues;
        this.allowedManagedPrintOptionsApplied.mediaType = true;
      }
    }

    if (this.capabilities_.printer.duplex &&
        this.managedPrintOptions_.duplex?.allowedValues &&
        this.managedPrintOptions_.duplex.allowedValues.length > 0) {
      const allowedDuplexValues =
          this.managedPrintOptions_.duplex.allowedValues;

      const allowedSupportedValues =
          this.capabilities_.printer.duplex.option.filter((supportedValue) => {
            return allowedDuplexValues.some((allowedValue) => {
              return supportedValue.type ===
                  managedPrintOptionsDuplexToCdd(allowedValue);
            });
          });

      if (allowedSupportedValues.length > 0 &&
          allowedSupportedValues.length <
              this.capabilities_.printer.duplex.option.length) {
        this.capabilities_.printer.duplex.option = allowedSupportedValues;
        this.allowedManagedPrintOptionsApplied.duplex = true;
      }
    }

    // Applying restrictions on the color setting only makes sense if the
    // printer supports printing both in black&white and in color.
    if (this.hasColorCapability &&
        this.managedPrintOptions_.color?.allowedValues &&
        this.managedPrintOptions_.color.allowedValues.length > 0) {
      const allowedColorValues = this.managedPrintOptions_.color.allowedValues;

      const allowedSupportedValues =
          this.capabilities_.printer.color!.option.filter((supportedValue) => {
            return allowedColorValues.some((allowedValue) => {
              const typesToLookFor =
                  allowedValue ? COLOR_TYPES : MONOCHROME_TYPES;
              return typesToLookFor.includes(supportedValue.type!);
            });
          });

      if (allowedSupportedValues.length > 0 &&
          allowedSupportedValues.length <
              this.capabilities_.printer.color!.option.length) {
        this.capabilities_.printer.color!.option = allowedSupportedValues!;
        this.allowedManagedPrintOptionsApplied.color = true;
      }
    }

    if (this.capabilities_.printer.dpi &&
        this.managedPrintOptions_.dpi?.allowedValues &&
        this.managedPrintOptions_.dpi.allowedValues.length > 0) {
      const allowedDpiValues = this.managedPrintOptions_.dpi.allowedValues;

      const allowedSupportedValues =
          this.capabilities_.printer.dpi.option.filter((supportedValue) => {
            return allowedDpiValues.some((allowedValue) => {
              return supportedValue.horizontal_dpi ===
                  allowedValue.horizontal &&
                  supportedValue.vertical_dpi === allowedValue.vertical;
            });
          });

      if (allowedSupportedValues.length > 0 &&
          allowedSupportedValues.length <
              this.capabilities_.printer.dpi.option.length) {
        this.capabilities_.printer.dpi.option = allowedSupportedValues!;
        this.allowedManagedPrintOptionsApplied.dpi = true;
      }
    }

    if (this.capabilities_.printer.vendor_capability &&
        this.managedPrintOptions_.quality?.allowedValues &&
        this.managedPrintOptions_.quality.allowedValues.length > 0) {
      const allowedQualityValues =
          this.managedPrintOptions_.quality.allowedValues;

      const printQualityCapability =
          this.capabilities_.printer.vendor_capability.find(o => {
            return o.id === IPP_PRINT_QUALITY;
          });
      if (printQualityCapability?.select_cap?.option) {
        const allowedSupportedValues =
            printQualityCapability.select_cap.option.filter(
                (supportedValue) => {
                  return allowedQualityValues.some((allowedValue) => {
                    return supportedValue.value ===
                        managedPrintOptionsQualityToIpp(allowedValue);
                  });
                });
        if (allowedSupportedValues.length > 0 &&
            allowedSupportedValues.length <
                printQualityCapability.select_cap.option.length) {
          printQualityCapability.select_cap.option = allowedSupportedValues;
          this.allowedManagedPrintOptionsApplied.quality = true;
        }
      }
    }
  }

  /** @return Path to the SVG for the destination's icon. */
  get icon(): string {
    if (this.id_ === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return 'print-preview:save-to-drive';
    }
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
   * @return Native color model of the destination.
   */
  getNativeColorModel(isColor: boolean): number {
    // For printers without capability, native color model is ignored.
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return isColor ? ColorMode.COLOR : ColorMode.GRAY;
    }
    const selected = this.getColor(isColor);
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

  /**
   * @return Color option value of the destination with the given binary color
   * value. Returns null if the destination doesn't support such color value.
   */
  getColor(isColor: boolean): ColorOption|null {
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
   * @return Media size value of the destination with the given width and height
   * values. Returns undefined if there is no such media size value.
   */
  getMediaSize(width: number, height: number): MediaSizeOption|undefined {
    return this.capabilities?.printer.media_size?.option.find(o => {
      return o.width_microns === width && o.height_microns === height;
    });
  }

  /**
   * @return Media type value of the destination with the given vendor id.
   * Returns undefined if there is no such media type value.
   */
  getMediaType(vendorId: string): MediaTypeOption|undefined {
    return this.capabilities?.printer.media_type?.option.find(o => {
      return o.vendor_id === vendorId;
    });
  }

  /**
   * @return DPI (Dots per Inch) value of the destination with the given
   * horizontal and vertical resolutions. Returns undefined if there is no such
   * DPI value.
   */
  getDpi(horizontal: number, vertical: number): DpiOption|undefined {
    return this.capabilities?.printer.dpi?.option.find(o => {
      return o.horizontal_dpi === horizontal && o.vertical_dpi === vertical;
    });
  }

  /**
   * @return Returns true if the current printing destination supports the given
   * duplex value. Returns false in all other cases.
   */
  supportsDuplex(duplex: DuplexType): boolean {
    const availableDuplexOptions = this.capabilities?.printer.duplex?.option;
    if (!availableDuplexOptions) {
      // There are no duplex capabilities reported by the printer.
      return false;
    }

    return availableDuplexOptions.some(o => {
      return o.type === duplex;
    });
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
  SAVE_TO_DRIVE_CROS = 'Save to Drive CrOS',
}

/* Unique identifier for the Save as PDF destination */
export const PDF_DESTINATION_KEY: string =
    `${GooglePromotedDestinationId.SAVE_AS_PDF}/${DestinationOrigin.LOCAL}/`;

/* Unique identifier for the Save to Drive CrOS destination */
export const SAVE_TO_DRIVE_CROS_DESTINATION_KEY: string =
    `${GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS}/${
        DestinationOrigin.LOCAL}/`;
