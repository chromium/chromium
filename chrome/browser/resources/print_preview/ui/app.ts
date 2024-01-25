// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './print_preview_vars.css.js';
import '../strings.m.js';
import '../data/document_info.js';
import './sidebar.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {isMac, isWindows} from 'chrome://resources/js/platform.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import {DestinationOrigin, PrinterType} from '../data/destination.js';
import type {DocumentSettings, PrintPreviewDocumentInfoElement} from '../data/document_info.js';
import type {Margins} from '../data/margins.js';
import {MeasurementSystem} from '../data/measurement_system.js';
import type {PrintPreviewModelElement} from '../data/model.js';
import {DuplexMode, whenReady} from '../data/model.js';
import type {PrintableArea} from '../data/printable_area.js';
// <if expr="is_chromeos">
import {computePrinterState, PrintAttemptOutcome, PrinterState} from '../data/printer_status_cros.js';
// </if>
import type {Size} from '../data/size.js';
import type {PrintPreviewStateElement} from '../data/state.js';
import {Error, State} from '../data/state.js';
import type {NativeInitialSettings, NativeLayer} from '../native_layer.js';
import {NativeLayerImpl} from '../native_layer.js';
// <if expr="is_chromeos">
import {NativeLayerCrosImpl} from '../native_layer_cros.js';

// </if>

import {getTemplate} from './app.html.js';
import {DestinationState} from './destination_settings.js';
import type {PrintPreviewPreviewAreaElement} from './preview_area.js';
import {PreviewAreaState} from './preview_area.js';
import {SettingsMixin} from './settings_mixin.js';
import type {PrintPreviewSidebarElement} from './sidebar.js';

export interface PrintPreviewAppElement {
  $: {
    documentInfo: PrintPreviewDocumentInfoElement,
    model: PrintPreviewModelElement,
    previewArea: PrintPreviewPreviewAreaElement,
    sidebar: PrintPreviewSidebarElement,
    state: PrintPreviewStateElement,
  };
}

const PrintPreviewAppElementBase =
    WebUiListenerMixin(SettingsMixin(PolymerElement));

export class PrintPreviewAppElement extends PrintPreviewAppElementBase {
  static get is() {
    return 'print-preview-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      state: {
        type: Number,
        observer: 'onStateChanged_',
      },

      controlsManaged_: {
        type: Boolean,
        computed: 'computeControlsManaged_(destinationsManaged_, ' +
            'settingsManaged_, maxSheets_)',
      },

      destination_: Object,

      destinationsManaged_: {
        type: Boolean,
        value: false,
      },

      destinationState_: {
        type: Number,
        observer: 'onDestinationStateChange_',
      },

      documentSettings_: Object,

      error_: {
        type: Number,
        observer: 'onErrorChange_',
      },

      margins_: Object,

      pageSize_: Object,

      previewState_: {
        type: String,
        observer: 'onPreviewStateChange_',
      },

      printableArea_: Object,

      settingsManaged_: {
        type: Boolean,
        value: false,
      },

      measurementSystem_: {
        type: Object,
        value: null,
      },

      maxSheets_: Number,
    };
  }

  state: State;
  private controlsManaged_: boolean;
  private destination_: Destination;
  private destinationsManaged_: boolean;
  private destinationState_: DestinationState;
  private documentSettings_: DocumentSettings;
  private error_: Error;
  private margins_: Margins;
  private pageSize_: Size;
  private previewState_: PreviewAreaState;
  private printableArea_: PrintableArea;
  private settingsManaged_: boolean;
  private measurementSystem_: MeasurementSystem|null;
  private maxSheets_: number;

  private nativeLayer_: NativeLayer|null = null;
  private tracker_: EventTracker = new EventTracker();
  private cancelled_: boolean = false;
  private printRequested_: boolean = false;
  private startPreviewWhenReady_: boolean = false;
  private showSystemDialogBeforePrint_: boolean = false;
  private openPdfInPreview_: boolean = false;
  private isInKioskAutoPrintMode_: boolean = false;
  private whenReady_: Promise<void>|null = null;
  private openDialogs_: CrDialogElement[] = [];

  constructor() {
    super();

    // Regular expression that captures the leading slash, the content and the
    // trailing slash in three different groups.
    const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
    const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
    if (path !== '/') {  // There are no subpages in Print Preview.
      window.history.replaceState(undefined /* stateObject */, '', '/');
    }
  }

  override ready() {
    super.ready();

    FocusOutlineManager.forDocument(document);
  }

  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');
    this.nativeLayer_ = NativeLayerImpl.getInstance();
    this.addWebUiListener('cr-dialog-open', this.onCrDialogOpen_.bind(this));
    this.addWebUiListener('close', this.onCrDialogClose_.bind(this));
    this.addWebUiListener('print-failed', this.onPrintFailed_.bind(this));
    this.addWebUiListener(
        'print-preset-options', this.onPrintPresetOptions_.bind(this));
    this.tracker_.add(window, 'keydown', this.onKeyDown_.bind(this));
    this.$.previewArea.setPluginKeyEventCallback(this.onKeyDown_.bind(this));
    this.whenReady_ = whenReady();
    this.nativeLayer_.getInitialSettings().then(
        this.onInitialSettingsSet_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
    this.whenReady_ = null;
  }

  private onSidebarFocus_() {
    this.$.previewArea.hideToolbar();
  }

  /**
   * Consume escape and enter key presses and ctrl + shift + p. Delegate
   * everything else to the preview area.
   */
  private onKeyDown_(e: KeyboardEvent) {
    // Escape key closes the topmost dialog that is currently open within
    // Print Preview. If no such dialog exists, then the Print Preview dialog
    // itself is closed.
    if (e.key === 'Escape' && !hasKeyModifiers(e)) {
      // Don't close the Print Preview dialog if there is a child dialog open.
      if (this.openDialogs_.length !== 0) {
        // Manually cancel the dialog, since we call preventDefault() to prevent
        // views from closing the Print Preview dialog.
        const dialogToClose = this.openDialogs_[this.openDialogs_.length - 1];
        dialogToClose.cancel();
        e.preventDefault();
        return;
      }

      // On non-mac with toolkit-views, ESC key is handled by C++-side instead
      // of JS-side.
      if (isMac) {
        this.close_();
        e.preventDefault();
      }

      // <if expr="is_chromeos">
      this.recordCancelMetricCros_();
      // </if>

      return;
    }

    // On Mac, Cmd+Period should close the print dialog.
    if (isMac && e.key === '.' && e.metaKey) {
      this.close_();
      e.preventDefault();
      return;
    }

    // Ctrl + Shift + p / Mac equivalent. Doesn't apply on Chrome OS.
    // On Linux/Windows, shift + p means that e.key will be 'P' with caps lock
    // off or 'p' with caps lock on.
    // On Mac, alt + p means that e.key will be unicode 03c0 (pi).
    // <if expr="not is_chromeos">
    if (e.key === 'P' || e.key === 'p' || e.key === '\u03c0') {
      if ((isMac && e.metaKey && e.altKey && !e.shiftKey && !e.ctrlKey) ||
          (!isMac && e.shiftKey && e.ctrlKey && !e.altKey && !e.metaKey)) {
        // Don't use system dialog if the link isn't available.
        if (!this.$.sidebar.systemDialogLinkAvailable()) {
          return;
        }

        // Don't try to print with system dialog on Windows if the document is
        // not ready, because we send the preview document to the printer on
        // Windows.
        if (!isWindows || this.state === State.READY) {
          this.onPrintWithSystemDialog_();
        }
        e.preventDefault();
        return;
      }
    }
    // </if>

    if ((e.key === 'Enter' || e.key === 'NumpadEnter') &&
        this.state === State.READY && this.openDialogs_.length === 0) {
      const activeElementTag = (e.composedPath()[0] as HTMLElement).tagName;
      if (['CR-BUTTON', 'BUTTON', 'SELECT', 'A', 'CR-CHECKBOX'].includes(
              activeElementTag)) {
        return;
      }

      this.onPrintRequested_();
      e.preventDefault();
      return;
    }

    // Pass certain directional keyboard events to the PDF viewer.
    this.$.previewArea.handleDirectionalKeyEvent(e);
  }

  private onCrDialogOpen_(e: Event) {
    this.openDialogs_.push(e.composedPath()[0] as CrDialogElement);
  }

  private onCrDialogClose_(e: Event) {
    // Note: due to event re-firing in cr_dialog.js, this event will always
    // appear to be coming from the outermost child dialog.
    // TODO(rbpotter): Fix event re-firing so that the event comes from the
    // dialog that has been closed, and add an assertion that the removed
    // dialog matches e.composedPath()[0].
    if ((e.composedPath()[0] as HTMLElement).nodeName === 'CR-DIALOG') {
      this.openDialogs_.pop();
    }
  }

  private onInitialSettingsSet_(settings: NativeInitialSettings) {
    if (!this.whenReady_) {
      // This element and its corresponding model were detached while waiting
      // for the callback. This can happen in tests; return early.
      return;
    }
    this.whenReady_.then(() => {
      this.$.documentInfo.init(
          settings.previewModifiable, settings.previewIsFromArc,
          settings.documentTitle, settings.documentHasSelection);
      this.$.model.setStickySettings(settings.serializedAppStateStr);
      this.$.model.setPolicySettings(settings.policies);
      this.measurementSystem_ = new MeasurementSystem(
          settings.thousandsDelimiter, settings.decimalDelimiter,
          settings.unitType);
      this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
      this.$.sidebar.init(
          settings.isInAppKioskMode, settings.printerName,
          settings.serializedDefaultDestinationSelectionRulesStr,
          settings.pdfPrinterDisabled, settings.isDriveMounted || false);
      this.destinationsManaged_ = settings.destinationsManaged;
      this.isInKioskAutoPrintMode_ = settings.isInKioskAutoPrintMode;

      // This is only visible in the task manager.
      let title = document.head.querySelector('title');
      if (!title) {
        title = document.createElement('title');
        document.head.appendChild(title);
      }
      title.textContent = settings.documentTitle;
    });
  }

  /**
   * @return Whether any of the print preview settings or destinations
   *     are managed.
   */
  private computeControlsManaged_(): boolean {
    // If |this.maxSheets_| equals to 0, no sheets limit policy is present.
    return this.destinationsManaged_ || this.settingsManaged_ ||
        this.maxSheets_ > 0;
  }

  private onDestinationStateChange_() {
    switch (this.destinationState_) {
      case DestinationState.SET:
        if (this.state !== State.NOT_READY &&
            this.state !== State.FATAL_ERROR) {
          this.$.state.transitTo(State.NOT_READY);
        }
        break;
      case DestinationState.UPDATED:
        if (!this.$.model.initialized()) {
          this.$.model.applyStickySettings();
        }

        this.$.model.applyDestinationSpecificPolicies();

        this.startPreviewWhenReady_ = true;

        if (this.state === State.NOT_READY &&
            this.destination_.type !== PrinterType.PDF_PRINTER) {
          this.nativeLayer_!.recordBooleanHistogram(
              'PrintPreview.TransitionedToReadyState', true);
        }

        this.$.state.transitTo(State.READY);
        break;
      case DestinationState.ERROR:
        let newState = State.ERROR;
        // <if expr="is_chromeos">
        if (this.error_ === Error.NO_DESTINATIONS) {
          newState = State.FATAL_ERROR;
        }
        // </if>

        if (this.state === State.NOT_READY &&
            this.destination_.type !== PrinterType.PDF_PRINTER) {
          this.nativeLayer_!.recordBooleanHistogram(
              'PrintPreview.TransitionedToReadyState', false);
        }

        this.$.state.transitTo(newState);
        break;
      default:
        break;
    }
  }

  /**
   * @param e Event containing the new sticky settings.
   */
  private onStickySettingChanged_(e: CustomEvent<string>) {
    this.nativeLayer_!.saveAppState(e.detail);
  }

  private onPreviewSettingChanged_() {
    if (this.state === State.READY) {
      this.$.previewArea.startPreview(false);
      this.startPreviewWhenReady_ = false;
    } else {
      this.startPreviewWhenReady_ = true;
    }
  }

  private onStateChanged_() {
    if (this.state === State.READY) {
      if (this.startPreviewWhenReady_) {
        this.$.previewArea.startPreview(false);
        this.startPreviewWhenReady_ = false;
      }
      if (this.isInKioskAutoPrintMode_ || this.printRequested_) {
        this.onPrintRequested_();
        // Reset in case printing fails.
        this.printRequested_ = false;
      }
    } else if (this.state === State.CLOSING) {
      this.remove();
      this.nativeLayer_!.dialogClose(this.cancelled_);
    } else if (this.state === State.HIDDEN) {
      if (this.destination_.type !== PrinterType.PDF_PRINTER) {
        // Only hide the preview for local, non PDF destinations.
        this.nativeLayer_!.hidePreview();
      }
    } else if (this.state === State.PRINTING) {
      // <if expr="is_chromeos">
      if (this.destination_.type === PrinterType.PDF_PRINTER) {
        NativeLayerCrosImpl.getInstance().recordPrintAttemptOutcome(
            PrintAttemptOutcome.PDF_PRINT_ATTEMPTED);
      }
      // </if>

      const whenPrintDone =
          this.nativeLayer_!.doPrint(this.$.model.createPrintTicket(
              this.destination_, this.openPdfInPreview_,
              this.showSystemDialogBeforePrint_));
      const onError = this.destination_.type === PrinterType.PDF_PRINTER ?
          this.onFileSelectionCancel_.bind(this) :
          this.onPrintFailed_.bind(this);
      whenPrintDone.then(this.close_.bind(this), onError);
    }
  }

  private onErrorChange_() {
    if (this.error_ !== Error.NONE) {
      this.nativeLayer_!.recordInHistogram(
          'PrintPreview.StateError', this.error_, Error.MAX_BUCKET);
    }
  }

  private onPrintRequested_() {
    if (this.state === State.NOT_READY) {
      this.printRequested_ = true;
      return;
    }

    this.$.state.transitTo(
        this.$.previewArea.previewLoaded() ? State.PRINTING : State.HIDDEN);
  }

  private onCancelRequested_() {
    // <if expr="is_chromeos">
    this.recordCancelMetricCros_();
    // </if>
    this.cancelled_ = true;
    this.$.state.transitTo(State.CLOSING);
  }

  // <if expr="is_chromeos">
  /** Records the Print Preview state when cancel is requested. */
  private recordCancelMetricCros_() {
    let printAttemptOutcome = null;
    if (this.state !== State.READY) {
      // Print button is disabled when state !== READY.
      printAttemptOutcome = PrintAttemptOutcome.CANCELLED_PRINT_BUTTON_DISABLED;
    } else if (!this.$.sidebar.printerExistsInDisplayedDestinations()) {
      printAttemptOutcome = PrintAttemptOutcome.CANCELLED_NO_PRINTERS_AVAILABLE;
    } else if (this.destination_.origin === DestinationOrigin.CROS) {
      // Fetch and record printer state.
      switch (computePrinterState(this.destination_.printerStatusReason)) {
        case PrinterState.GOOD:
          printAttemptOutcome =
              PrintAttemptOutcome.CANCELLED_PRINTER_GOOD_STATUS;
          break;
        case PrinterState.ERROR:
          printAttemptOutcome =
              PrintAttemptOutcome.CANCELLED_PRINTER_ERROR_STATUS;
          break;
        case PrinterState.UNKNOWN:
          printAttemptOutcome =
              PrintAttemptOutcome.CANCELLED_PRINTER_UNKNOWN_STATUS;
          break;
      }
    } else {
      printAttemptOutcome =
          PrintAttemptOutcome.CANCELLED_OTHER_PRINTERS_AVAILABLE;
    }

    if (printAttemptOutcome !== null) {
      NativeLayerCrosImpl.getInstance().recordPrintAttemptOutcome(
          printAttemptOutcome);
    }
  }
  // </if>

  /**
   * @param e The event containing the new validity.
   */
  private onSettingValidChanged_(e: CustomEvent<boolean>) {
    if (e.detail) {
      this.$.state.transitTo(State.READY);
    } else {
      this.error_ = Error.INVALID_TICKET;
      this.$.state.transitTo(State.ERROR);
    }
  }

  private onFileSelectionCancel_() {
    this.$.state.transitTo(State.READY);
  }

  // <if expr="not is_chromeos">
  private onPrintWithSystemDialog_() {
    // <if expr="is_win">
    this.showSystemDialogBeforePrint_ = true;
    this.onPrintRequested_();
    // </if>
    // <if expr="not is_win">
    this.nativeLayer_!.showSystemDialog();
    this.$.state.transitTo(State.SYSTEM_DIALOG);
    // </if>
  }
  // </if>

  // <if expr="is_macosx">
  private onOpenPdfInPreview_() {
    this.openPdfInPreview_ = true;
    this.$.previewArea.setOpeningPdfInPreview();
    this.onPrintRequested_();
  }
  // </if>

  /**
   * Called when printing to an extension printer fails.
   * @param httpError The HTTP error code, or -1 or a string describing
   *     the error, if not an HTTP error.
   */
  private onPrintFailed_(httpError: number|string) {
    console.warn('Printing failed with error code ' + httpError);
    this.error_ = Error.PRINT_FAILED;
    this.$.state.transitTo(State.FATAL_ERROR);
  }

  private onPreviewStateChange_() {
    switch (this.previewState_) {
      case PreviewAreaState.DISPLAY_PREVIEW:
      case PreviewAreaState.OPEN_IN_PREVIEW_LOADED:
        if (this.state === State.HIDDEN) {
          this.$.state.transitTo(State.PRINTING);
        }
        break;
      case PreviewAreaState.ERROR:
        if (this.state !== State.ERROR && this.state !== State.FATAL_ERROR) {
          this.$.state.transitTo(
              this.error_ === Error.INVALID_PRINTER ? State.ERROR :
                                                      State.FATAL_ERROR);
        }
        break;
      default:
        break;
    }
  }

  /**
   * Updates printing options according to source document presets.
   * @param disableScaling Whether the document disables scaling.
   * @param copies The default number of copies from the document.
   * @param duplex The default duplex setting from the document.
   */
  private onPrintPresetOptions_(
      disableScaling: boolean, copies: number, duplex: DuplexMode) {
    if (disableScaling) {
      this.$.documentInfo.updateIsScalingDisabled(true);
    }

    if (copies > 0 && this.getSetting('copies').available) {
      this.setSetting('copies', copies, true);
    }

    if (duplex === DuplexMode.UNKNOWN_DUPLEX_MODE) {
      return;
    }

    if (this.getSetting('duplex').available) {
      this.setSetting(
          'duplex',
          duplex === DuplexMode.LONG_EDGE || duplex === DuplexMode.SHORT_EDGE,
          true);
    }

    if (duplex !== DuplexMode.SIMPLEX &&
        this.getSetting('duplexShortEdge').available) {
      this.setSetting(
          'duplexShortEdge', duplex === DuplexMode.SHORT_EDGE, true);
    }
  }

  /**
   * @param e Contains the new preview request ID.
   */
  private onPreviewStart_(e: CustomEvent<number>) {
    this.$.documentInfo.inFlightRequestId = e.detail;
  }

  private close_() {
    this.$.state.transitTo(State.CLOSING);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-app': PrintPreviewAppElement;
  }
}

customElements.define(PrintPreviewAppElement.is, PrintPreviewAppElement);
