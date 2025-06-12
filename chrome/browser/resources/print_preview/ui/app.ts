// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '../data/document_info.js';
import '../data/model.js';
import '../data/state.js';
import './preview_area.js';
import './sidebar.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {isMac, isWindows} from 'chrome://resources/js/platform.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';
import {PrinterType} from '../data/destination.js';
import type {DocumentSettings, PrintPreviewDocumentInfoElement} from '../data/document_info.js';
import {createDocumentSettings} from '../data/document_info.js';
import type {Margins} from '../data/margins.js';
import {MeasurementSystem} from '../data/measurement_system.js';
import type {PrintPreviewModelElement} from '../data/model.js';
import {DuplexMode, whenReady} from '../data/model.js';
import {Size} from '../data/size.js';
import type {PrintPreviewStateElement} from '../data/state.js';
import {Error, State} from '../data/state.js';
import type {NativeInitialSettings, NativeLayer} from '../native_layer.js';
import {NativeLayerImpl} from '../native_layer.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
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
    WebUiListenerMixinLit(SettingsMixin(CrLitElement));

export class PrintPreviewAppElement extends PrintPreviewAppElementBase {
  static get is() {
    return 'print-preview-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Number},
      controlsManaged_: {type: Boolean},
      destination_: {type: Object},
      destinationsManaged_: {type: Boolean},
      documentSettings_: {type: Object},
      error_: {type: Number},
      margins_: {type: Object},
      pageSize_: {type: Object},
      settingsManaged_: {type: Boolean},
      measurementSystem_: {type: Object},
    };
  }

  accessor state: State = State.NOT_READY;
  protected accessor controlsManaged_: boolean = false;
  protected accessor destination_: Destination|null = null;
  private accessor destinationsManaged_: boolean = false;
  protected accessor documentSettings_: DocumentSettings =
      createDocumentSettings();
  protected accessor error_: Error|null = null;
  protected accessor margins_: Margins|null = null;
  protected accessor pageSize_: Size = new Size(612, 792);
  protected accessor settingsManaged_: boolean = false;
  protected accessor measurementSystem_: MeasurementSystem|null = null;

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

  override firstUpdated() {
    FocusOutlineManager.forDocument(document);
  }

  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');
    this.nativeLayer_ = NativeLayerImpl.getInstance();
    this.addWebUiListener('cr-dialog-open', this.onCrDialogOpen_.bind(this));
    this.addWebUiListener('close', this.onCrDialogClose_.bind(this));
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('destinationsManaged_') ||
        changedPrivateProperties.has('settingsManaged_')) {
      this.controlsManaged_ =
          this.destinationsManaged_ || this.settingsManaged_;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('state')) {
      this.updateUiForStateChange_();
    }

    if (changedPrivateProperties.has('error_')) {
      if (this.error_ !== null && this.error_ !== Error.NONE) {
        this.nativeLayer_!.recordInHistogram(
            'PrintPreview.StateError', this.error_, Error.MAX_BUCKET);
      }
    }
  }

  protected onSidebarFocus_() {
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
      if (this.openDialogs_.length > 0) {
        // Manually cancel the dialog, since we call preventDefault() to prevent
        // views from closing the Print Preview dialog.
        const dialogToClose = this.openDialogs_[this.openDialogs_.length - 1]!;
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

    if ((e.key === 'Enter' || e.key === 'NumpadEnter') &&
        this.state === State.READY && this.openDialogs_.length === 0) {
      const activeElement = e.composedPath()[0] as HTMLElement | undefined;
      // activeElement may be undefined if this is a forwarded key event from
      // the plugin. Print Preview conventionally does not trigger a print for
      // Enter when the plugin is focused
      if (!activeElement) {
        return;
      }

      const activeElementTag = activeElement.tagName;
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
          settings.previewModifiable, settings.documentTitle,
          settings.documentHasSelection);
      this.$.model.setStickySettings(settings.serializedAppStateStr);
      this.$.model.setPolicySettings(settings.policies);
      this.measurementSystem_ = new MeasurementSystem(
          settings.thousandsDelimiter, settings.decimalDelimiter,
          settings.unitType);
      this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
      this.$.sidebar.init(
          settings.isInAppKioskMode, settings.printerName,
          settings.serializedDefaultDestinationSelectionRulesStr,
          settings.pdfPrinterDisabled);
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

  protected onDestinationStateChanged_(
      e: CustomEvent<{value: DestinationState}>) {
    const destinationState = e.detail.value;

    switch (destinationState) {
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

        this.$.model.applyPoliciesOnDestinationUpdate();

        this.startPreviewWhenReady_ = true;

        if (this.state === State.NOT_READY &&
            this.destination_!.type !== PrinterType.PDF_PRINTER) {
          this.nativeLayer_!.recordBooleanHistogram(
              'PrintPreview.TransitionedToReadyState', true);
        }

        this.$.state.transitTo(State.READY);
        break;
      case DestinationState.ERROR:
        if (this.state === State.NOT_READY &&
            this.destination_!.type !== PrinterType.PDF_PRINTER) {
          this.nativeLayer_!.recordBooleanHistogram(
              'PrintPreview.TransitionedToReadyState', false);
        }

        this.$.state.transitTo(State.ERROR);
        break;
      default:
        break;
    }
  }

  /**
   * @param e Event containing the new sticky settings.
   */
  protected onStickySettingChanged_(e: CustomEvent<string>) {
    this.nativeLayer_!.saveAppState(e.detail);
  }

  protected async onPreviewSettingChanged_() {
    if (this.state === State.READY) {
      // Need to wait for rendering to finish, to ensure that the `destination`
      // is synced across print-preview-app, print-preview-model and
      // print-preview-area.
      await this.updateComplete;
      assert(this.destination_!.id === this.$.previewArea.destination!.id);
      assert(this.destination_!.id === this.$.model.destination!.id);
      this.$.previewArea.startPreview(false);
      this.startPreviewWhenReady_ = false;
    } else {
      this.startPreviewWhenReady_ = true;
    }
  }

  private updateUiForStateChange_() {
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
    } else if (this.state === State.PRINT_PENDING) {
      assert(this.destination_);
      if (this.destination_.type !== PrinterType.PDF_PRINTER) {
        // Only hide the preview for local, non PDF destinations.
        this.nativeLayer_!.hidePreview();
        this.$.state.transitTo(State.HIDDEN);
      }
    } else if (this.state === State.PRINTING) {
      assert(this.destination_);
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

  protected onPrintRequested_() {
    if (this.state === State.NOT_READY) {
      this.printRequested_ = true;
      return;
    }

    this.$.state.transitTo(
        this.$.previewArea.previewLoaded() ? State.PRINTING :
                                             State.PRINT_PENDING);
  }

  protected onCancelRequested_() {
    this.cancelled_ = true;
    this.$.state.transitTo(State.CLOSING);
  }

  /**
   * @param e The event containing the new validity.
   */
  protected onSettingValidChanged_(e: CustomEvent<boolean>) {
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

  protected onPrintWithSystemDialog_() {
    // <if expr="is_win">
    this.showSystemDialogBeforePrint_ = true;
    this.onPrintRequested_();
    // </if>
    // <if expr="not is_win">
    this.nativeLayer_!.showSystemDialog();
    this.$.state.transitTo(State.SYSTEM_DIALOG);
    // </if>
  }

  // <if expr="is_macosx">
  protected onOpenPdfInPreview_() {
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

  protected onPreviewStateChanged_(e: CustomEvent<{value: PreviewAreaState}>) {
    const previewState = e.detail.value;

    switch (previewState) {
      case PreviewAreaState.DISPLAY_PREVIEW:
      case PreviewAreaState.OPEN_IN_PREVIEW_LOADED:
        if (this.state === State.PRINT_PENDING || this.state === State.HIDDEN) {
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
  protected onPreviewStart_(e: CustomEvent<number>) {
    this.$.documentInfo.inFlightRequestId = e.detail;
  }

  private close_() {
    this.$.state.transitTo(State.CLOSING);
  }

  protected onDestinationChanged_(e: CustomEvent<{value: Destination}>) {
    this.destination_ = e.detail.value;
  }

  protected onDestinationCapabilitiesChanged_() {
    this.$.model.updateSettingsFromDestination();
  }

  protected onStateChanged_(e: CustomEvent<{value: State}>) {
    this.state = e.detail.value;
  }

  protected onErrorChanged_(e: CustomEvent<{value: Error}>) {
    this.error_ = e.detail.value;
  }

  protected onSettingsManagedChanged_(e: CustomEvent<{value: boolean}>) {
    this.settingsManaged_ = e.detail.value;
  }

  protected onDocumentSettingsChanged_(
      e: CustomEvent<{value: DocumentSettings}>) {
    this.documentSettings_ = e.detail.value;
  }

  protected onMarginsChanged_(e: CustomEvent<{value: Margins}>) {
    this.margins_ = e.detail.value;
  }

  protected onPageSizeChanged_(e: CustomEvent<{value: Size}>) {
    this.pageSize_ = e.detail.value;
  }
}

export type AppElement = PrintPreviewAppElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-app': PrintPreviewAppElement;
  }
}

customElements.define(PrintPreviewAppElement.is, PrintPreviewAppElement);
