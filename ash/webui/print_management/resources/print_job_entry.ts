// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './icons.html.js';
import './print_management_fonts.css.js';
import './print_management_shared.css.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FocusRowMixin} from 'chrome://resources/js/cr/ui/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
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
  static get is() {
    return 'print-job-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      jobEntry: {
        type: Object,
      },

      jobTitle_: {
        type: String,
        computed: 'decodeString16_(jobEntry.title)',
      },

      printerName_: {
        type: String,
        computed: 'decodeString16_(jobEntry.printerName)',
      },

      creationTime_: {
        type: String,
        computed: 'computeDate_(jobEntry.creationTime)',
      },

      completionStatus_: {
        type: String,
        computed: 'computeCompletionStatus_(jobEntry.completedInfo)',
      },

      // Empty if there is no ongoing error.
      ongoingErrorStatus_: {
        type: String,
        computed: 'getOngoingErrorStatus_(jobEntry.printerErrorCode)',
      },

      /**
       * A representation in fraction form of pages printed versus total number
       * of pages to be printed. E.g. 5/7 (5 pages printed / 7 total pages to
       * print).
       */
      readableProgress_: {
        type: String,
        computed: 'computeReadableProgress_(jobEntry.activePrintJobInfo)',
      },

      jobEntryAriaLabel_: {
        type: String,
        computed: 'getJobEntryAriaLabel_(jobEntry, jobTitle_, printerName_, ' +
            'creationTime_, completionStatus_, ' +
            'jobEntry.activePrintJobinfo.printedPages, jobEntry.numberOfPages)',
      },

      // This is only updated by media queries from window width changes.
      showFullOngoingStatus_: Boolean,

      fileIcon_: {
        type: String,
        computed: 'computeFileIcon_(jobTitle_)',
      },

      fileIconClass_: {
        type: String,
        computed: 'computeFileIconClass_(fileIcon_)',
      },

    };
  }

  jobEntry: PrintJobInfo;
  private mojoInterfaceProvider_: PrintingMetadataProviderInterface;
  private jobTitle_: string;
  private printerName_: string;
  private creationTime_: string;
  private completionStatus_: string;
  private ongoingErrorStatus_: string;
  private readableProgress_: string;
  private jobEntryAriaLabel_: string;
  private showFullOngoingStatus_: boolean;
  private fileIcon_: string;
  private fileIconClass_: string;

  static get observers() {
    return [
      'printJobEntryDataChanged_(jobTitle_, printerName_, creationTime_, ' +
          'completionStatus_)',
    ];
  }

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getMetadataProvider();

    this.addEventListener('click', () => this.onClick_());
  }

  // Return private property this.fileIconClass for usage in browser tests.
  getFileIconClass(): string {
    return this.fileIconClass_;
  }

  /**
   * Check if any elements with the class "overflow-ellipsis" needs to
   * add/remove the title attribute.
   */
  private printJobEntryDataChanged_() {
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

  private onClick_() {
    if (!this.shadowRoot) {
      return;
    }

    // Since the status or cancel button has the focus-row-control attribute,
    // this will trigger the iron-list focus behavior and highlight the entire
    // entry.
    if (this.isCompletedPrintJob_()) {
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

  override connectedCallback() {
    super.connectedCallback();

    IronA11yAnnouncer.requestAvailability();
  }

  private computeCompletionStatus_(): string {
    if (!this.jobEntry.completedInfo) {
      return '';
    }

    return this.convertStatusToString_(
        this.jobEntry.completedInfo.completionStatus);
  }

  private computeReadableProgress_(): string {
    if (!this.jobEntry.activePrintJobInfo) {
      return '';
    }

    return loadTimeData.getStringF(
        'printedPagesFraction',
        this.jobEntry.activePrintJobInfo.printedPages.toString(),
        this.jobEntry.numberOfPages.toString());
  }

  private onCancelPrintJobClicked_() {
    this.mojoInterfaceProvider_.cancelPrintJob(this.jobEntry.id)
        .then((() => this.onPrintJobCanceled_()));
  }

  private onPrintJobCanceled_() {
    // TODO(crbug/1093527): Handle error case in which attempted cancellation
    // failed. Need to discuss with UX on error states.
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail:
          {text: loadTimeData.getStringF('cancelledPrintJob', this.jobTitle_)},
    }));
    this.dispatchEvent(new CustomEvent(
        'remove-print-job',
        {bubbles: true, composed: true, detail: this.jobEntry.id}));
  }

  private decodeString16_(arr: String16): string {
    return arr.data.map(ch => String.fromCodePoint(ch)).join('');
  }

  /**
   * Converts mojo time to JS time. Returns "Today" if |mojoTime| is at the
   * current day.
   */
  private computeDate_(mojoTime: Time): string {
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

  private convertStatusToString_(mojoCompletionStatus:
                                     PrintJobCompletionStatus): string {
    switch (mojoCompletionStatus) {
      case PrintJobCompletionStatus.kFailed:
        return this.getFailedStatusString_(this.jobEntry.printerErrorCode);
      case PrintJobCompletionStatus.kCanceled:
        return loadTimeData.getString('completionStatusCanceled');
      case PrintJobCompletionStatus.kPrinted:
        return loadTimeData.getString('completionStatusPrinted');
      default:
        assertNotReached();
        return loadTimeData.getString('unknownPrinterError');
    }
  }

  /**
   * Returns true if the job entry is a completed print job.
   * Returns false otherwise.
   */
  private isCompletedPrintJob_(): boolean {
    return !!this.jobEntry.completedInfo && !this.jobEntry.activePrintJobInfo;
  }

  private getJobEntryAriaLabel_(): string {
    if (!this.jobEntry || this.jobEntry.numberOfPages === undefined ||
        this.printerName_ === undefined || this.jobTitle_ === undefined ||
        !this.creationTime_) {
      return '';
    }

    // |completionStatus_| and |jobEntry.activePrintJobInfo| are mutually
    // exclusive and one of which has to be non-null. Assert that if
    // |completionStatus_| is non-null that |jobEntry.activePrintJobInfo| is
    // null and vice-versa.
    assert(
        this.completionStatus_ ? !this.jobEntry.activePrintJobInfo :
                                 this.jobEntry.activePrintJobInfo);

    if (this.isCompletedPrintJob_()) {
      return loadTimeData.getStringF(
          'completePrintJobLabel', this.jobTitle_, this.printerName_,
          this.creationTime_, this.completionStatus_);
    }
    if (this.ongoingErrorStatus_) {
      return loadTimeData.getStringF(
          'stoppedOngoingPrintJobLabel', this.jobTitle_, this.printerName_,
          this.creationTime_, this.ongoingErrorStatus_);
    }
    return loadTimeData.getStringF(
        'ongoingPrintJobLabel', this.jobTitle_, this.printerName_,
        this.creationTime_,
        this.jobEntry.activePrintJobInfo ?
            this.jobEntry.activePrintJobInfo.printedPages.toString() :
            '',
        this.jobEntry.numberOfPages.toString());
  }

  /**
   * Returns the percentage, out of 100, of the pages printed versus total
   * number of pages.
   */
  private computePrintPagesProgress_(
      printedPages: number,
      totalPages: number,
      ): number {
    assert(printedPages >= 0);
    assert(totalPages > 0);
    assert(printedPages <= totalPages);
    return (printedPages * 100) / totalPages;
  }

  /**
   * The full icon name provided by the containing iron-iconset-svg
   * (i.e. [iron-iconset-svg name]:[SVG <g> tag id]) for a given file.
   * This is a best effort approach, as we are only given the file name and
   * not necessarily its extension.
   */
  private computeFileIcon_(): string {
    const fileExtension = getFileExtensionIconName(this.jobTitle_);
    // It's valid for a file to have '.' in its name and not be its extension.
    // If this is the case and we don't have a non-generic file icon, attempt to
    // see if this is a Google file.
    if (fileExtension && fileExtension !== GENERIC_FILE_EXTENSION_ICON) {
      return fileExtension;
    }
    const gfileExtension = getGFileIconName(this.jobTitle_);
    if (gfileExtension) {
      return gfileExtension;
    }

    return GENERIC_FILE_EXTENSION_ICON;
  }

  /**
   * Uses file-icon SVG id to determine correct class to apply for file icon.
   */
  private computeFileIconClass_(): string {
    const iconClass = ICON_CLASS_MAP.get(this.fileIcon_);
    return `flex-center ${iconClass}`;
  }

  private getFailedStatusString_(
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
      default:
        assertNotReached();
        return loadTimeData.getString('unknownPrinterError');
    }
  }

  private getOngoingErrorStatus_(
      mojoPrinterErrorCode: PrinterErrorCode,
      ): string {
    if (this.isCompletedPrintJob_()) {
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
      case PrinterErrorCode.kPrinterUnreachable:
        assertNotReached();
        return loadTimeData.getString('unknownPrinterErrorStopped');
      default:
        assertNotReached();
        return loadTimeData.getString('unknownPrinterErrorStopped');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-job-entry': PrintJobEntryElement;
  }
}

customElements.define(PrintJobEntryElement.is, PrintJobEntryElement);
