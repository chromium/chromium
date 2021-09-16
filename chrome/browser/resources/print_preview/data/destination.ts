// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, isLacros} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

// <if expr="chromeos or lacros">
import {NativeLayerCrosImpl} from '../native_layer_cros.js';
// </if>

import {Cdd, ColorCapability, ColorOption, CopiesCapability} from './cdd.js';

// <if expr="chromeos or lacros">
import {getStatusReasonFromPrinterStatus, PrinterStatus, PrinterStatusReason} from './printer_status_cros.js';
// </if>

/**
 * Enumeration of the types of destinations.
 */
export enum DestinationType {
  GOOGLE = 'google',
  GOOGLE_PROMOTED = 'google_promoted',
  LOCAL = 'local',
  MOBILE = 'mobile',
}

/**
 * Enumeration of the origin types for cloud destinations.
 */
export enum DestinationOrigin {
  LOCAL = 'local',
  COOKIES = 'cookies',
  // <if expr="chromeos or lacros">
  DEVICE = 'device',
  // </if>
  // Note: Privet is deprecated, but used to filter any legacy entries in the
  // recent destinations, since we can't guarantee all recent privet printers
  // have been overridden.
  PRIVET = 'privet',
  EXTENSION = 'extension',
  CROS = 'chrome_os',
}

/**
 * Cloud Print origins.
 */
export const CloudOrigins: DestinationOrigin[] = [
  DestinationOrigin.COOKIES,
  // <if expr="chromeos or lacros">
  DestinationOrigin.DEVICE,
  // </if>
];

/**
 * Enumeration of the connection statuses of printer destinations.
 */
export enum DestinationConnectionStatus {
  DORMANT = 'DORMANT',
  OFFLINE = 'OFFLINE',
  ONLINE = 'ONLINE',
  UNKNOWN = 'UNKNOWN',
  UNREGISTERED = 'UNREGISTERED',
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
 * Enumeration specifying the status of a destination's 2018 certificate.
 * Values UNKNOWN and YES are returned directly by the GCP server.
 */
export enum DestinationCertificateStatus {
  // Destination is not a cloud printer or no status was retrieved.
  NONE = 'NONE',
  // Printer does not have a valid 2018 certificate. Currently unused, to be
  // sent by GCP server.
  NO = 'NO',
  // Printer may or may not have a valid certificate. Sent by GCP server.
  UNKNOWN = 'UNKNOWN',
  // Printer has a valid 2018 certificate. Sent by GCP server.
  YES = 'YES',
}

/**
 * Enumeration of color modes used by Chromium.
 */
export enum ColorMode {
  GRAY = 1,
  COLOR = 2,
}

export type RecentDestination = {
  id: string,
  origin: DestinationOrigin,
  account: string,
  capabilities: Cdd|null,
  displayName: string,
  extensionId: string,
  extensionName: string,
  icon?: string,
};

/**
 * Creates a |RecentDestination| to represent |destination| in the app
 * state.
 */
export function makeRecentDestination(destination: Destination):
    RecentDestination {
  return {
    id: destination.id,
    origin: destination.origin,
    account: destination.account || '',
    capabilities: destination.capabilities,
    displayName: destination.displayName || '',
    extensionId: destination.extensionId || '',
    extensionName: destination.extensionName || '',
    icon: destination.icon || '',
  };
}

/**
 * @return key that maps to a destination with the selected |id|,
 *     |origin|, and |account|.
 */
export function createDestinationKey(
    id: string, origin: DestinationOrigin, account: string): string {
  return `${id}/${origin}/${account}`;
}

/**
 * @return A key that maps to a destination with parameters matching
 *     |recentDestination|.
 */
export function createRecentDestinationKey(
    recentDestination: RecentDestination): string {
  return createDestinationKey(
      recentDestination.id, recentDestination.origin,
      recentDestination.account);
}

export type DestinationOptionalParams = {
  tags?: string[],
  isOwned?: boolean,
  isEnterprisePrinter?: boolean,
  account?: string,
  lastAccessTime?: number,
  cloudID?: string,
  provisionalType?: DestinationProvisionalType,
  extensionId?: string,
  extensionName?: string,
  description?: string,
  certificateStatus?: DestinationCertificateStatus,
};

/**
 * Print destination data object that holds data for both local and cloud
 * destinations.
 */
export class Destination {
  /**
   * ID of the destination.
   */
  private id_: string;

  /**
   * Type of the destination.
   */
  private type_: DestinationType;

  /**
   * Origin of the destination.
   */
  private origin_: DestinationOrigin;

  /**
   * Display name of the destination.
   */
  private displayName_: string;

  /**
   * Tags associated with the destination.
   */
  private tags_: string[];

  /**
   * Print capabilities of the destination.
   */
  private capabilities_: Cdd|null = null;

  /**
   * Whether the destination is owned by the user.
   */
  private isOwned_: boolean;

  /**
   * Whether the destination is an enterprise policy controlled printer.
   */
  private isEnterprisePrinter_: boolean;

  /**
   * Account this destination is registered for, if known.
   */
  private account_: string;

  /**
   * Cache of destination location fetched from tags.
   */
  private location_: string|null = null;

  /**
   * Printer description.
   */
  private description_: string;

  /**
   * Connection status of the destination.
   */
  private connectionStatus_: DestinationConnectionStatus;

  /**
   * Number of milliseconds since the epoch when the printer was last
   * accessed.
   */
  private lastAccessTime_: number;

  /**
   * Cloud ID for Privet printers.
   */
  private cloudID_: string;

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
   * Printer 2018 certificate status
   * @private {DestinationCertificateStatus}
   */
  private certificateStatus_: DestinationCertificateStatus;

  // <if expr="chromeos or lacros">
  /**
   * EULA url for printer's PPD. Empty string indicates no provided EULA.
   */
  private eulaUrl_: string = '';

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

  /**
   * List of capability types considered color.
   */
  private COLOR_TYPES_: string[] = ['STANDARD_COLOR', 'CUSTOM_COLOR'];

  /**
   * List of capability types considered monochrome.
   */
  private MONOCHROME_TYPES_: string[] =
      ['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'];

  constructor(
      id: string, type: DestinationType, origin: DestinationOrigin,
      displayName: string, connectionStatus: DestinationConnectionStatus,
      opt_params?: DestinationOptionalParams) {
    this.id_ = id;
    this.type_ = type;
    this.origin_ = origin;
    this.displayName_ = displayName || '';
    this.tags_ = (opt_params && opt_params.tags) || [];
    this.isOwned_ = (opt_params && opt_params.isOwned) || false;
    this.isEnterprisePrinter_ =
        (opt_params && opt_params.isEnterprisePrinter) || false;
    this.account_ = (opt_params && opt_params.account) || '';
    this.description_ = (opt_params && opt_params.description) || '';
    this.connectionStatus_ = connectionStatus;
    this.lastAccessTime_ =
        (opt_params && opt_params.lastAccessTime) || Date.now();
    this.cloudID_ = (opt_params && opt_params.cloudID) || '';
    this.extensionId_ = (opt_params && opt_params.extensionId) || '';
    this.extensionName_ = (opt_params && opt_params.extensionName) || '';
    this.provisionalType_ = (opt_params && opt_params.provisionalType) ||
        DestinationProvisionalType.NONE;
    this.certificateStatus_ = opt_params && opt_params.certificateStatus ||
        DestinationCertificateStatus.NONE;

    assert(
        this.provisionalType_ !==
                DestinationProvisionalType.NEEDS_USB_PERMISSION ||
            this.isExtension,
        'Provisional USB destination only supprted with extension origin.');
  }

  get id(): string {
    return this.id_;
  }

  get type(): DestinationType {
    return this.type_;
  }

  get origin(): DestinationOrigin {
    return this.origin_;
  }

  get displayName(): string {
    return this.displayName_;
  }

  /**
   * @return Whether the user owns the destination. Only applies to
   *     cloud-based destinations.
   */
  get isOwned(): boolean {
    return this.isOwned_;
  }

  /**
   * @return Account this destination is registered for, if known.
   */
  get account(): string {
    return this.account_;
  }

  /** @return Whether the destination is local (vs cloud-based). */
  get isLocal(): boolean {
    return this.origin_ === DestinationOrigin.LOCAL ||
        this.origin_ === DestinationOrigin.EXTENSION ||
        this.origin_ === DestinationOrigin.CROS;
  }

  /**
   * @return Whether the destination is an extension managed printer.
   */
  get isExtension(): boolean {
    return this.origin_ === DestinationOrigin.EXTENSION;
  }

  /**
   * @return The location of the destination, or an empty string if
   *     the location is unknown.
   */
  get location(): string {
    if (this.location_ === null) {
      this.location_ = '';
      this.tags_.some(tag => {
        return LOCATION_TAG_PREFIXES.some(prefix => {
          if (tag.startsWith(prefix)) {
            this.location_ = tag.substring(prefix.length) || '';
            return true;
          } else {
            return false;
          }
        });
      });
    }
    return this.location_;
  }

  /**
   * @return The description of the destination, or an empty string,
   *     if it was not provided.
   */
  get description(): string {
    return this.description_;
  }

  /**
   * @return Most relevant string to help user to identify this
   *     destination.
   */
  get hint(): string {
    if (this.id_ === GooglePromotedDestinationId.DOCS) {
      return this.account_;
    }
    return this.location || this.extensionName || this.description;
  }

  get tags(): string[] {
    return this.tags_.slice(0);
  }

  get cloudID(): string {
    return this.cloudID_;
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

  // <if expr="chromeos or lacros">
  get eulaUrl(): string {
    return this.eulaUrl_;
  }

  set eulaUrl(eulaUrl: string) {
    this.eulaUrl_ = eulaUrl;
  }

  /**
   * @return The printer status reason for a local Chrome OS printer.
   */
  get printerStatusReason(): PrinterStatusReason|null {
    return this.printerStatusReason_;
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

            // If this is the second printer status attempt, record the result.
            if (this.printerStatusRetrySent_) {
              NativeLayerCrosImpl.getInstance()
                  .recordPrinterStatusRetrySuccessHistogram(
                      !isPrinterUnreachable);
            }
          }
          return Promise.resolve(this.key);
        });
  }

  /**
   * Pause for a set timeout then retry the printer status request.
   * @return Promise with destination key.
   * @private
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

  // </if>

  get connectionStatus(): DestinationConnectionStatus {
    return this.connectionStatus_;
  }

  set connectionStatus(status: DestinationConnectionStatus) {
    this.connectionStatus_ = status;
  }

  /**
   * @return Whether the destination has an invalid 2018 certificate.
   */
  get hasInvalidCertificate(): boolean {
    return this.certificateStatus_ === DestinationCertificateStatus.NO;
  }

  /**
   * @return Whether the destination should display an invalid
   *     certificate UI warning in the selection dialog and cause a UI
   *     warning to appear in the preview area when selected.
   */
  get shouldShowInvalidCertificateError(): boolean {
    return this.certificateStatus_ === DestinationCertificateStatus.NO &&
        !loadTimeData.getBoolean('isEnterpriseManaged');
  }

  /** @return Whether the destination is considered offline. */
  get isOffline(): boolean {
    return [
      DestinationConnectionStatus.OFFLINE, DestinationConnectionStatus.DORMANT
    ].includes(this.connectionStatus_);
  }

  /**
   * @return Whether the destination is offline or has an invalid certificate.
   */
  get isOfflineOrInvalid(): boolean {
    return this.isOffline || this.shouldShowInvalidCertificateError;
  }

  /** @return Whether the destination is ready to be selected. */
  get readyForSelection(): boolean {
    return (!(isChromeOS || isLacros) ||
            this.origin_ !== DestinationOrigin.CROS ||
            this.capabilities_ !== null) &&
        !this.isProvisional;
  }

  /**
   * @return Human readable status for a destination that is offline
   *     or has a bad certificate.
   */
  get connectionStatusText(): string {
    if (!this.isOfflineOrInvalid) {
      return '';
    }
    const offlineDurationMs = Date.now() - this.lastAccessTime_;
    let statusMessageId;
    if (this.shouldShowInvalidCertificateError) {
      statusMessageId = 'noLongerSupported';
    } else if (offlineDurationMs > 31622400000.0) {  // One year.
      statusMessageId = 'offlineForYear';
    } else if (offlineDurationMs > 2678400000.0) {  // One month.
      statusMessageId = 'offlineForMonth';
    } else if (offlineDurationMs > 604800000.0) {  // One week.
      statusMessageId = 'offlineForWeek';
    } else {
      statusMessageId = 'offline';
    }
    return loadTimeData.getString(statusMessageId);
  }

  /**
   * @return Number of milliseconds since the epoch when the printer
   *     was last accessed.
   */
  get lastAccessTime(): number {
    return this.lastAccessTime_;
  }

  /** @return Path to the SVG for the destination's icon. */
  get icon(): string {
    // <if expr="chromeos or lacros">
    if (this.id_ === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return 'print-preview:save-to-drive';
    }
    // </if>
    if (this.id_ === GooglePromotedDestinationId.DOCS) {
      return 'print-preview:save-to-drive';
    }
    if (this.id_ === GooglePromotedDestinationId.SAVE_AS_PDF) {
      return 'cr:insert-drive-file';
    }
    if (this.isEnterprisePrinter) {
      return 'print-preview:business';
    }
    if (this.isLocal) {
      return 'print-preview:print';
    }
    if (this.type_ === DestinationType.MOBILE) {
      return 'print-preview:smartphone';
    }
    if (this.isOwned_) {
      return 'print-preview:print';
    }
    return 'print-preview:printer-shared';
  }

  /**
   * @return Properties (besides display name) to match search queries against.
   */
  get extraPropertiesToMatch(): string[] {
    return [this.location, this.description];
  }

  /**
   * Matches a query against the destination.
   * @param query Query to match against the destination.
   * @return {@code true} if the query matches this destination,
   *     {@code false} otherwise.
   */
  matches(query: RegExp): boolean {
    return !!this.displayName_.match(query) ||
        !!this.extensionName_.match(query) ||
        this.extraPropertiesToMatch.some(p => p.match(query));
  }

  get provisionalType(): DestinationProvisionalType {
    return this.provisionalType_;
  }

  get certificateStatus(): DestinationCertificateStatus {
    return this.certificateStatus_;
  }

  get isProvisional(): boolean {
    return this.provisionalType_ !== DestinationProvisionalType.NONE;
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
      const type = assert(option.type!);
      hasColor = hasColor || this.COLOR_TYPES_.includes(type);
      hasMonochrome = hasMonochrome || this.MONOCHROME_TYPES_.includes(type);
    });
    return hasColor && hasMonochrome;
  }

  /**
   * @param isColor Whether to use a color printing mode.
   * @return Selected color option.
   */
  getSelectedColorOption(isColor: boolean): ColorOption|null {
    const typesToLookFor = isColor ? this.COLOR_TYPES_ : this.MONOCHROME_TYPES_;
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
    // For non-local printers or printers without capability, native color
    // model is ignored.
    const capability = this.colorCapability_();
    if (!capability || !capability.option || !this.isLocal) {
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
    return `${this.id_}/${this.origin_}/${this.account_}`;
  }
}

/**
 * Prefix of the location destination tag.
 */
const LOCATION_TAG_PREFIXES: string[] =
    ['__cp__location=', '__cp__printer-location='];

/**
 * Enumeration of Google-promoted destination IDs.
 * @enum {string}
 */
export enum GooglePromotedDestinationId {
  DOCS = '__google__docs',
  SAVE_AS_PDF = 'Save as PDF',
  // <if expr="chromeos or lacros">
  SAVE_TO_DRIVE_CROS = 'Save to Drive CrOS',
  // </if>
}

/* Unique identifier for the Save as PDF destination */
export const PDF_DESTINATION_KEY: string =
    `${GooglePromotedDestinationId.SAVE_AS_PDF}/${DestinationOrigin.LOCAL}/`;

// <if expr="chromeos or lacros">
/* Unique identifier for the Save to Drive CrOS destination */
export const SAVE_TO_DRIVE_CROS_DESTINATION_KEY: string =
    `${GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS}/${
        DestinationOrigin.LOCAL}/`;
// </if>
