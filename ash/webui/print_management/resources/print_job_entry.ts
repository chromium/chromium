// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './icons.html.js';
import './print_management_fonts.css.js';
import './print_management_shared.css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {FocusRowMixin} from 'chrome://resources/ash/common/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getMetadataProvider} from './mojo_interface_provider.js';
import {getTemplate} from './print_job_entry.html.js';
import {PrinterErrorCode, PrintingMetadataProviderInterface, PrintJobCompletionStatus, PrintJobInfo} from './printing_manager.mojom-webui.js';

const GENERIC_FILE_EXTENSION_ICON = 'print-management:file-generic';

// Lookup table maps icons to the correct display class.
const ICON_CLASS_MAP = new Map([
  ['print-management:file-gdoc', 'file-icon-blue'],
  ['print-management:file-word', 'file-icon-blue'],
  ['print-management:file-generic', 'file-icon-gray'],
  ['print-management:file-excel', 'file-icon-green'],
  ['print-management:file-gform', 'file-icon-green'],
  ['print-management:file-gsheet', 'file-icon-green'],
  ['print-management:file-image', 'file-icon-red'],
  ['print-management:file-gdraw', 'file-icon-red'],
  ['print-management:file-gslide', 'file-icon-yellow'],
  ['print-management:file-pdf', 'file-icon-red'],
  ['print-management:file-ppt', 'file-icon-red'],
]);

// Converts a mojo time to a JS time.
function convertMojoTimeToJS(mojoTime: Time): Date {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

// Returns true if |date| is today, false otherwise.
function isToday(date: Date): boolean {
  const todayDate = new Date();
  return date.getDate() === todayDate.getDate() &&
      date.getMonth() === todayDate.getMonth() &&
      date.getFullYear() === todayDate.getFullYear();
}

/**
 * Best effort attempt of finding the file icon name based off of the file's
 * name extension. If extension is not available, return an empty string. If
 * file name does have an extension but we don't have an icon for it, return a
 * generic icon name.
 */
function getFileExtensionIconName(fileName: string): string {
  // Get file extension delimited by '.'.
  const ext = fileName.split('.').pop();

  // Return empty string if file has no extension.
  if (ext === fileName || !ext) {
    return '';
  }

  switch (ext) {
    case 'pdf':
    case 'xps':
      return 'print-management:file-pdf';
    case 'doc':
    case 'docx':
    case 'docm':
      return 'print-management:file-word';
    case 'png':
    case 'jpeg':
    case 'gif':
    case 'raw':
    case 'heic':
    case 'svg':
      return 'print-management:file-image';
    case 'ppt':
    case 'pptx':
    case 'pptm':
      return 'print-management:file-ppt';
    case 'xlsx':
    case 'xltx':
    case 'xlr':
      return 'print-management:file-excel';
    default:
      return GENERIC_FILE_EXTENSION_ICON;
  }
}

/**
 * Best effort to get the file icon name for a Google-file
 * (e.g. Google docs, Google sheets, Google forms). Returns an empty
 * string if |fileName| is not a Google-file.
 */
function getGFileIconName(fileName: string): string {
  // Google-files are delimited by '-'.
  const ext = fileName.split('-').pop();

  // Return empty string if this doesn't have a Google-file delimiter.
  if (ext === fileName || !ext) {
    return '';
  }

  // Eliminate space that appears infront of Google-file file names.
  const gExt = ext.substring(1);
  switch (gExt) {
    case 'Google Docs':
      return 'print-management:file-gdoc';
    case 'Google Sheets':
      return 'print-management:file-gsheet';
    case 'Google Forms':
      return 'print-management:file-gform';
    case 'Google Drawings':
      return 'print-management:file-gdraw';
    case 'Google Slides':
      return 'print-management:file-gslide';
    default:
      return '';
  }
}

/**
 * @fileoverview
 * 'print-job-entry' is contains a single print job entry and is used as a list
 * item.
 */

const PrintJobEntryElementBase = FocusRowMixin(I18nMixin(PolymerElement));

export class PrintJobEntryElement extends PrintJobEntryElementBase {
  static get is(): string {
    return 'print-job-entry';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      jobEntry: {
        type: Object,
      },

      jobTitle: {
        type: String,
        computed: 'decodeString16(jobEntry.title)',
      },

      printerName: {
        type: String,
        computed: 'decodeString16(jobEntry.printerName)',
      },

      creationTime: {
        type: String,
        computed: 'computeDate(jobEntry.creationTime)',
      },

      completionStatus: {
        type: String,
        computed: 'computeCompletionStatus(jobEntry.completedInfo)',
      },

      // Empty if there is no ongoing error.
      ongoingErrorStatus: {
        type: String,
        computed: 'getOngoingErrorStatus(jobEntry.printerErrorCode)',
      },

      /**
       * A representation in fraction form of pages printed versus total number
       * of pages to be printed. E.g. 5/7 (5 pages printed / 7 total pages to
       * print).
       */
      readableProgress: {
        type: String,
        computed: 'computeReadableProgress(jobEntry.activePrintJobInfo)',
      },

      jobEntryAriaLabel: {
        type: String,
        computed: 'getJobEntryAriaLabel(jobEntry, jobTitle, printerName, ' +
            'creationTime, completionStatus, ' +
            'jobEntry.activePrintJobinfo.printedPages, jobEntry.numberOfPages)',
      },

      // This is only updated by media queries from window width changes.
      showFullOngoingStatus: Boolean,

      fileIcon: {
        type: String,
        computed: 'computeFileIcon(jobTitle)',
      },

      fileIconClass: {
        type: String,
        computed: 'computeFileIconClass(fileIcon)',
      },

    };
  }

  jobEntry: PrintJobInfo;
  private mojoInterfaceProvider: PrintingMetadataProviderInterface;
  private jobTitle: string;
  private printerName: string;
  private creationTime: string;
  private completionStatus: string;
  private ongoingErrorStatus: string;
  private readableProgress: string;
  private jobEntryAriaLabel: string;
  private showFullOngoingStatus: boolean;
  private fileIcon: string;
  private fileIconClass: string;

  static get observers(): string[] {
    return [
      'printJobEntryDataChanged(jobTitle, printerName, creationTime, ' +
          'completionStatus)',
    ];
  }

  constructor() {
    super();

    this.mojoInterfaceProvider = getMetadataProvider();

    this.addEventListener('click', () => this.onClick());
  }

  // Return private property this.fileIconClass for usage in browser tests.
  getFileIconClass(): string {
    return this.fileIconClass;
  }

  /**
   * Check if any elements with the class "overflow-ellipsis" needs to
   * add/remove the title attribute.
   */
  private printJobEntryDataChanged(): void {
    if (!this.shadowRoot) {
      return;
    }

    Array
        .from(
            this.shadowRoot.querySelectorAll<HTMLElement>('.overflow-ellipsis'),
            )
        .forEach((e) => {
          // Checks if text is truncated
          if (e.offsetWidth < e.scrollWidth) {
            e.setAttribute('title', e.textContent || '');
          } else {
            e.removeAttribute('title');
          }
        });
  }

  private onClick(): void {
    if (!this.shadowRoot) {
      return;
    }

    // Since the status or cancel button has the focus-row-control attribute,
    // this will trigger the iron-list focus behavior and highlight the entire
    // entry.
    if (this.isCompletedPrintJob()) {
      this.shadowRoot.querySelector<HTMLElement>('#completionStatus')?.focus();
      return;
    }
    // Focus on the cancel button when clicking on the entry.
    this.shadowRoot
        .querySelector<HTMLElement>(
            '#cancelPrintJobButton',
            )
        ?.focus();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    IronA11yAnnouncer.requestAvailability();
  }

  private computeCompletionStatus(): string {
    if (!this.jobEntry.completedInfo) {
      return '';
    }

    return this.convertStatusToString(
        this.jobEntry.completedInfo.completionStatus);
  }

  private computeReadableProgress(): string {
    if (!this.jobEntry.activePrintJobInfo) {
      return '';
    }

    return loadTimeData.getStringF(
        'printedPagesFraction',
        this.jobEntry.activePrintJobInfo.printedPages.toString(),
        this.jobEntry.numberOfPages.toString());
  }

  private onCancelPrintJobClicked(): void {
    this.mojoInterfaceProvider.cancelPrintJob(this.jobEntry.id)
        .then((() => this.onPrintJobCanceled()));
  }

  private onPrintJobCanceled(): void {
    // TODO(crbug/1093527): Handle error case in which attempted cancellation
    // failed. Need to discuss with UX on error states.
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail:
          {text: loadTimeData.getStringF('cancelledPrintJob', this.jobTitle)},
    }));
    this.dispatchEvent(new CustomEvent(
        'remove-print-job',
        {bubbles: true, composed: true, detail: this.jobEntry.id}));
  }

  private decodeString16(arr: String16): string {
    return arr.data.map(ch => String.fromCodePoint(ch)).join('');
  }

  /**
   * Converts mojo time to JS time. Returns "Today" if |mojoTime| is at the
   * current day.
   */
  private computeDate(mojoTime: Time): string {
    const jsDate = convertMojoTimeToJS(mojoTime);
    // Date() is constructed with the current time in UTC. If the Date() matches
    // |jsDate|'s date, display the 12hour time of the current date.
    if (isToday(jsDate)) {
      return jsDate.toLocaleTimeString(
          /*locales=*/ undefined, {hour: 'numeric', minute: 'numeric'});
    }
    // Remove the day of the week from the date.
    return jsDate.toLocaleDateString(
        /*locales=*/ undefined,
        {month: 'short', day: 'numeric', year: 'numeric'});
  }

  private convertStatusToString(mojoCompletionStatus: PrintJobCompletionStatus):
      string {
    switch (mojoCompletionStatus) {
      case PrintJobCompletionStatus.kFailed:
        return this.getFailedStatusString(this.jobEntry.printerErrorCode);
      case PrintJobCompletionStatus.kCanceled:
        return loadTimeData.getString('completionStatusCanceled');
      case PrintJobCompletionStatus.kPrinted:
        return loadTimeData.getString('completionStatusPrinted');
      default:
        assertNotReached();
    }
  }

  /**
   * Returns true if the job entry is a completed print job.
   * Returns false otherwise.
   */
  private isCompletedPrintJob(): boolean {
    return !!this.jobEntry.completedInfo && !this.jobEntry.activePrintJobInfo;
  }

  private getJobEntryAriaLabel(): string {
    if (!this.jobEntry || this.jobEntry.numberOfPages === undefined ||
        this.printerName === undefined || this.jobTitle === undefined ||
        !this.creationTime) {
      return '';
    }

    // |completionStatus| and |jobEntry.activePrintJobInfo| are mutually
    // exclusive and one of which has to be non-null. Assert that if
    // |completionStatus| is non-null that |jobEntry.activePrintJobInfo| is
    // null and vice-versa.
    assert(
        this.completionStatus ? !this.jobEntry.activePrintJobInfo :
                                this.jobEntry.activePrintJobInfo);

    if (this.isCompletedPrintJob()) {
      return loadTimeData.getStringF(
          'completePrintJobLabel', this.jobTitle, this.printerName,
          this.creationTime, this.completionStatus);
    }
    if (this.ongoingErrorStatus) {
      return loadTimeData.getStringF(
          'stoppedOngoingPrintJobLabel', this.jobTitle, this.printerName,
          this.creationTime, this.ongoingErrorStatus);
    }
    return loadTimeData.getStringF(
        'ongoingPrintJobLabel', this.jobTitle, this.printerName,
        this.creationTime,
        this.jobEntry.activePrintJobInfo ?
            this.jobEntry.activePrintJobInfo.printedPages.toString() :
            '',
        this.jobEntry.numberOfPages.toString());
  }

  /**
   * Returns the percentage, out of 100, of the pages printed versus total
   * number of pages.
   */
  private computePrintPagesProgress(
      printedPages: number,
      totalPages: number,
      ): number {
    assert(printedPages >= 0);
    // TODO(b/235534580): Remove print statements once resolved.
    if (totalPages <= 0) {
      console.error('Total pages should be > 0. totalPages: ' + totalPages);
    }
    assert(totalPages > 0);
    if (printedPages > totalPages) {
      console.error(
          'Total pages should be more than printed pages. totalPages: ' +
          totalPages + ' printedPages: ' + printedPages);
    }
    assert(printedPages <= totalPages);
    return (printedPages * 100) / totalPages;
  }

  /**
   * The full icon name provided by the containing iron-iconset-svg
   * (i.e. [iron-iconset-svg name]:[SVG <g> tag id]) for a given file.
   * This is a best effort approach, as we are only given the file name and
   * not necessarily its extension.
   */
  private computeFileIcon(): string {
    const fileExtension = getFileExtensionIconName(this.jobTitle);
    // It's valid for a file to have '.' in its name and not be its extension.
    // If this is the case and we don't have a non-generic file icon, attempt to
    // see if this is a Google file.
    if (fileExtension && fileExtension !== GENERIC_FILE_EXTENSION_ICON) {
      return fileExtension;
    }
    const gfileExtension = getGFileIconName(this.jobTitle);
    if (gfileExtension) {
      return gfileExtension;
    }

    return GENERIC_FILE_EXTENSION_ICON;
  }

  /**
   * Uses file-icon SVG id to determine correct class to apply for file icon.
   */
  private computeFileIconClass(): string {
    const iconClass = ICON_CLASS_MAP.get(this.fileIcon);
    return `flex-center ${iconClass}`;
  }

  private getFailedStatusString(
      mojoPrinterErrorCode: PrinterErrorCode,
      ): string {
    switch (mojoPrinterErrorCode) {
      case PrinterErrorCode.kNoError:
        return loadTimeData.getString('completionStatusPrinted');
      case PrinterErrorCode.kPaperJam:
        return loadTimeData.getString('paperJam');
      case PrinterErrorCode.kOutOfPaper:
        return loadTimeData.getString('outOfPaper');
      case PrinterErrorCode.kOutOfInk:
        return loadTimeData.getString('outOfInk');
      case PrinterErrorCode.kDoorOpen:
        return loadTimeData.getString('doorOpen');
      case PrinterErrorCode.kPrinterUnreachable:
        return loadTimeData.getString('printerUnreachable');
      case PrinterErrorCode.kTrayMissing:
        return loadTimeData.getString('trayMissing');
      case PrinterErrorCode.kOutputFull:
        return loadTimeData.getString('outputFull');
      case PrinterErrorCode.kStopped:
        return loadTimeData.getString('stopped');
      case PrinterErrorCode.kFilterFailed:
        return loadTimeData.getString('filterFailed');
      case PrinterErrorCode.kUnknownError:
        return loadTimeData.getString('unknownPrinterError');
      case PrinterErrorCode.kClientUnauthorized:
        return loadTimeData.getString('clientUnauthorized');
      case PrinterErrorCode.kExpiredCertificate:
        return loadTimeData.getString('expiredCertificate');
      default:
        assertNotReached();
    }
  }

  private getOngoingErrorStatus(
      mojoPrinterErrorCode: PrinterErrorCode,
      ): string {
    if (this.isCompletedPrintJob()) {
      return '';
    }

    switch (mojoPrinterErrorCode) {
      case PrinterErrorCode.kNoError:
        return '';
      case PrinterErrorCode.kPaperJam:
        return loadTimeData.getString('paperJamStopped');
      case PrinterErrorCode.kOutOfPaper:
        return loadTimeData.getString('outOfPaperStopped');
      case PrinterErrorCode.kOutOfInk:
        return loadTimeData.getString('outOfInkStopped');
      case PrinterErrorCode.kDoorOpen:
        return loadTimeData.getString('doorOpenStopped');
      case PrinterErrorCode.kTrayMissing:
        return loadTimeData.getString('trayMissingStopped');
      case PrinterErrorCode.kOutputFull:
        return loadTimeData.getString('outputFullStopped');
      case PrinterErrorCode.kStopped:
        return loadTimeData.getString('stoppedGeneric');
      case PrinterErrorCode.kFilterFailed:
        return loadTimeData.getString('filterFailed');
      case PrinterErrorCode.kUnknownError:
        return loadTimeData.getString('unknownPrinterErrorStopped');
      case PrinterErrorCode.kClientUnauthorized:
        return loadTimeData.getString('clientUnauthorized');
      case PrinterErrorCode.kExpiredCertificate:
        return loadTimeData.getString('expiredCertificate');
      case PrinterErrorCode.kPrinterUnreachable:
        return loadTimeData.getString('printerUnreachableStopped');
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-job-entry': PrintJobEntryElement;
  }
}

customElements.define(PrintJobEntryElement.is, PrintJobEntryElement);
