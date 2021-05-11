// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './print_preview_vars_css.js';
import '../strings.m.js';
import './sidebar.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudPrintInterface, CloudPrintInterfaceErrorEventDetail, CloudPrintInterfaceEventType} from '../cloud_print_interface.js';
import {CloudPrintInterfaceImpl} from '../cloud_print_interface_impl.js';
import {Destination, DestinationOrigin} from '../data/destination.js';
import {getPrinterTypeForDestination, PrinterType} from '../data/destination_match.js';
import {DocumentSettings} from '../data/document_info.js';
import {Margins} from '../data/margins.js';
import {MeasurementSystem} from '../data/measurement_system.js';
import {DuplexMode, whenReady} from '../data/model.js';
import {PrintableArea} from '../data/printable_area.js';
import {Size} from '../data/size.js';
import {Error, State} from '../data/state.js';
import {NativeInitialSettings, NativeLayer, NativeLayerImpl} from '../native_layer.js';
// <if expr="chromeos">
import {NativeLayerCros, NativeLayerCrosImpl} from '../native_layer_cros.js';
// </if>

import {DestinationState} from './destination_settings.js';
import {PreviewAreaState} from './preview_area.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-app',

  _template: html`{__html_template__}`,

  behaviors: [
    SettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {!State} */
    state: {
      type: Number,
      observer: 'onStateChanged_',
    },

    /** @private {string} */
    cloudPrintErrorMessage_: String,

    /** @private {!CloudPrintInterface} */
    cloudPrintInterface_: Object,

    /** @private {boolean} */
    controlsManaged_: {
      type: Boolean,
      computed: 'computeControlsManaged_(destinationsManaged_, ' +
          'settingsManaged_, maxSheets_)',
    },

    /** @private {Destination} */
    destination_: Object,

    /** @private {boolean} */
    destinationsManaged_: {
      type: Boolean,
      value: false,
    },

    /** @private {!DestinationState} */
    destinationState_: {
      type: Number,
      observer: 'onDestinationStateChange_',
    },

    /** @private {DocumentSettings} */
    documentSettings_: Object,

    /** @private {!Error} */
    error_: Number,

    /** @private {Margins} */
    margins_: Object,

    /** @private {!Size} */
    pageSize_: Object,

    /** @private {!PreviewAreaState} */
    previewState_: {
      type: String,
      observer: 'onPreviewStateChange_',
    },

    /** @private {!PrintableArea} */
    printableArea_: Object,

    /** @private {boolean} */
    settingsManaged_: {
      type: Boolean,
      value: false,
    },

    /** @private {?MeasurementSystem} */
    measurementSystem_: {
      type: Object,
      value: null,
    },

    /** @private {number} */
    maxSheets_: Number,
  },

  listeners: {
    'cr-dialog-open': 'onCrDialogOpen_',
    'close': 'onCrDialogClose_',
  },

  /** @private {?NativeLayer} */
  nativeLayer_: null,

  // <if expr="chromeos">
  /** @private {?NativeLayerCros} */
  nativeLayerCros_: null,
  // </if>

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private {boolean} */
  cancelled_: false,

  /** @private {boolean} */
  printRequested_: false,

  /** @private {boolean} */
  startPreviewWhenReady_: false,

  /** @private {boolean} */
  showSystemDialogBeforePrint_: false,

  /** @private {boolean} */
  openPdfInPreview_: false,

  /** @private {boolean} */
  isInKioskAutoPrintMode_: false,

  /** @private {?Promise} */
  whenReady_: null,

  /** @private {!Array<!CrDialogElement>} */
  openDialogs_: [],

  /** @override */
  created() {
    // Regular expression that captures the leading slash, the content and the
    // trailing slash in three different groups.
    const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
    const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
    if (path !== '/') {  // There are no subpages in Print Preview.
      window.history.replaceState(undefined /* stateObject */, '', '/');
    }
  },

  /** @override */
  ready() {
    FocusOutlineManager.forDocument(document);
  },

  /** @override */
  attached() {
    document.documentElement.classList.remove('loading');
    this.nativeLayer_ = NativeLayerImpl.getInstance();
    // <if expr="chromeos">
    this.nativeLayerCros_ = NativeLayerCrosImpl.getInstance();
    // </if>
    this.addWebUIListener('print-failed', this.onPrintFailed_.bind(this));
    this.addWebUIListener(
        'print-preset-options', this.onPrintPresetOptions_.bind(this));
    this.tracker_.add(window, 'keydown', this.onKeyDown_.bind(this));
    this.$.previewArea.setPluginKeyEventCallback(this.onKeyDown_.bind(this));
    this.whenReady_ = whenReady();
    this.nativeLayer_.getInitialSettings().then(
        this.onInitialSettingsSet_.bind(this));
  },

  /** @override */
  detached() {
    this.tracker_.removeAll();
    this.whenReady_ = null;
  },

  /** @private */
  onSidebarFocus_() {
    this.$.previewArea.hideToolbar();
  },

  /**
   * Consume escape and enter key presses and ctrl + shift + p. Delegate
   * everything else to the preview area.
   * @param {!KeyboardEvent} e The keyboard event.
   * @private
   */
  onKeyDown_(e) {
    // Escape key closes the topmost dialog that is currently open within
    // Print Preview. If no such dialog exists, then the Print Preview dialog
    // itself is closed.
    if (e.code === 'Escape' && !hasKeyModifiers(e)) {
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

      // <if expr="chromeos">
      if (this.destination_ &&
          this.destination_.origin === DestinationOrigin.CROS) {
        this.nativeLayerCros_.recordPrinterStatusHistogram(
            this.destination_.printerStatusReason, false);
      }
      // </if>
      return;
    }

    // On Mac, Cmd+Period should close the print dialog.
    if (isMac && e.code === 'Period' && e.metaKey) {
      this.close_();
      e.preventDefault();
      return;
    }

    // Ctrl + Shift + p / Mac equivalent.
    if (e.code === 'KeyP') {
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

    if ((e.code === 'Enter' || e.code === 'NumpadEnter') &&
        this.state === State.READY && this.openDialogs_.length === 0) {
      const activeElementTag = e.path[0].tagName;
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
  },

  /**
   * @param {!Event} e The cr-dialog-open event.
   * @private
   */
  onCrDialogOpen_(e) {
    this.openDialogs_.push(
        /** @type {!CrDialogElement} */ (e.composedPath()[0]));
  },

  /**
   * @param {!Event} e The close event.
   * @private
   */
  onCrDialogClose_(e) {
    // Note: due to event re-firing in cr_dialog.js, this event will always
    // appear to be coming from the outermost child dialog.
    // TODO(rbpotter): Fix event re-firing so that the event comes from the
    // dialog that has been closed, and add an assertion that the removed
    // dialog matches e.composedPath()[0].
    if (e.composedPath()[0].nodeName === 'CR-DIALOG') {
      this.openDialogs_.pop();
    }
  },

  /**
   * @param {!NativeInitialSettings} settings
   * @private
   */
  onInitialSettingsSet_(settings) {
    if (!this.whenReady_) {
      // This element and its corresponding model were detached while waiting
      // for the callback. This can happen in tests; return early.
      return;
    }
    this.whenReady_.then(() => {
      // The cloud print interface should be initialized before initializing the
      // sidebar, so that cloud printers can be selected automatically.
      if (settings.cloudPrintURL) {
        this.initializeCloudPrint_(
            settings.cloudPrintURL, settings.isInAppKioskMode,
            settings.uiLocale);
      }
      this.$.documentInfo.init(
          settings.previewModifiable, settings.previewIsFromArc,
          settings.previewIsPdf, settings.documentTitle,
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
  },

  /**
   * Called when Google Cloud Print integration is enabled.
   * @param {string} cloudPrintUrl The URL to use for cloud print servers.
   * @param {boolean} appKioskMode Whether the browser is in app kiosk mode.
   * @param {string} uiLocale The UI locale.
   * @private
   */
  initializeCloudPrint_(cloudPrintUrl, appKioskMode, uiLocale) {
    assert(!this.cloudPrintInterface_);
    this.cloudPrintInterface_ = CloudPrintInterfaceImpl.getInstance();
    this.cloudPrintInterface_.configure(cloudPrintUrl, appKioskMode, uiLocale);
    this.tracker_.add(
        assert(this.cloudPrintInterface_).getEventTarget(),
        CloudPrintInterfaceEventType.SUBMIT_DONE, this.close_.bind(this));
    this.tracker_.add(
        assert(this.cloudPrintInterface_).getEventTarget(),
        CloudPrintInterfaceEventType.SUBMIT_FAILED,
        this.onCloudPrintError_.bind(this, appKioskMode));
  },

  /**
   * @return {boolean} Whether any of the print preview settings or destinations
   *     are managed.
   * @private
   */
  computeControlsManaged_() {
    // If |this.maxSheets_| equals to 0, no sheets limit policy is present.
    return this.destinationsManaged_ || this.settingsManaged_ ||
        this.maxSheets_ > 0;
  },

  /** @private */
  onDestinationStateChange_() {
    switch (this.destinationState_) {
      case DestinationState.SELECTED:
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
        this.$.state.transitTo(State.READY);
        break;
      case DestinationState.ERROR:
        let newState = State.ERROR;
        // <if expr="chromeos">
        if (this.error_ === Error.NO_DESTINATIONS) {
          newState = State.FATAL_ERROR;
        }
        // </if>
        this.$.state.transitTo(newState);
        break;
      default:
        break;
    }
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new sticky settings.
   * @private
   */
  onStickySettingChanged_(e) {
    this.nativeLayer_.saveAppState(e.detail);
  },

  /** @private */
  onPreviewSettingChanged_() {
    if (this.state === State.READY) {
      this.$.previewArea.startPreview(false);
      this.startPreviewWhenReady_ = false;
    } else {
      this.startPreviewWhenReady_ = true;
    }
  },

  /** @private */
  onStateChanged_() {
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
      this.nativeLayer_.dialogClose(this.cancelled_);
    } else if (this.state === State.HIDDEN) {
      if (this.destination_.isLocal &&
          getPrinterTypeForDestination(this.destination_) !==
              PrinterType.PDF_PRINTER) {
        // Only hide the preview for local, non PDF destinations.
        this.nativeLayer_.hidePreview();
      }
    } else if (this.state === State.PRINTING) {
      const destination = assert(this.destination_);
      const whenPrintDone =
          this.nativeLayer_.print(this.$.model.createPrintTicket(
              destination, this.openPdfInPreview_,
              this.showSystemDialogBeforePrint_));
      if (destination.isLocal) {
        const onError = getPrinterTypeForDestination(destination) ===
                PrinterType.PDF_PRINTER ?
            this.onFileSelectionCancel_.bind(this) :
            this.onPrintFailed_.bind(this);
        whenPrintDone.then(this.close_.bind(this), onError);
      } else {
        // Cloud print resolves when print data is returned to submit to cloud
        // print, or if print ticket cannot be read, no PDF data is found, or
        // PDF is oversized.
        whenPrintDone.then(
            this.onPrintToCloud_.bind(this), this.onPrintFailed_.bind(this));
      }
    }
  },

  /** @private */
  onPrintRequested_() {
    if (this.state === State.NOT_READY) {
      this.printRequested_ = true;
      return;
    }
    // <if expr="chromeos">
    if (this.destination_ &&
        this.destination_.origin === DestinationOrigin.CROS) {
      this.nativeLayerCros_.recordPrinterStatusHistogram(
          this.destination_.printerStatusReason, true);
    }
    // </if>
    this.$.state.transitTo(
        this.$.previewArea.previewLoaded() ? State.PRINTING : State.HIDDEN);
  },

  /** @private */
  onCancelRequested_() {
    // <if expr="chromeos">
    if (this.destination_ &&
        this.destination_.origin === DestinationOrigin.CROS) {
      this.nativeLayerCros_.recordPrinterStatusHistogram(
          this.destination_.printerStatusReason, false);
    }
    // </if>
    this.cancelled_ = true;
    this.$.state.transitTo(State.CLOSING);
  },

  /**
   * @param {!CustomEvent<boolean>} e The event containing the new validity.
   * @private
   */
  onSettingValidChanged_(e) {
    if (e.detail) {
      this.$.state.transitTo(State.READY);
    } else {
      this.error_ = Error.INVALID_TICKET;
      this.$.state.transitTo(State.ERROR);
    }
  },

  /** @private */
  onFileSelectionCancel_() {
    this.$.state.transitTo(State.READY);
  },

  /**
   * Called when the native layer has retrieved the data to print to Google
   * Cloud Print.
   * @param {string} data The body to send in the HTTP request.
   * @private
   */
  onPrintToCloud_(data) {
    assert(
        this.cloudPrintInterface_ !== null,
        'Google Cloud Print is not enabled');
    const destination = assert(this.destination_);
    this.cloudPrintInterface_.submit(
        destination, this.$.model.createCloudJobTicket(destination),
        this.documentSettings_.title, data);
  },

  // <if expr="not chromeos">
  /** @private */
  onPrintWithSystemDialog_() {
    // <if expr="is_win">
    this.showSystemDialogBeforePrint_ = true;
    this.onPrintRequested_();
    // </if>
    // <if expr="not is_win">
    this.nativeLayer_.showSystemDialog();
    this.$.state.transitTo(State.SYSTEM_DIALOG);
    // </if>
  },
  // </if>

  // <if expr="is_macosx">
  /** @private */
  onOpenPdfInPreview_() {
    this.openPdfInPreview_ = true;
    this.$.previewArea.setOpeningPdfInPreview();
    this.onPrintRequested_();
  },
  // </if>

  /**
   * Called when printing to a privet, cloud, or extension printer fails.
   * @param {*} httpError The HTTP error code, or -1 or a string describing
   *     the error, if not an HTTP error.
   * @private
   */
  onPrintFailed_(httpError) {
    console.warn('Printing failed with error code ' + httpError);
    this.error_ = Error.PRINT_FAILED;
    this.$.state.transitTo(State.FATAL_ERROR);
  },

  /** @private */
  onPreviewStateChange_() {
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
  },

  /**
   * Called when there was an error communicating with Google Cloud print.
   * Displays an error message in the print header.
   * @param {boolean} appKioskMode
   * @param {!CustomEvent<!CloudPrintInterfaceErrorEventDetail>}
   *     event Contains the error message.
   * @private
   */
  onCloudPrintError_(appKioskMode, event) {
    if (event.detail.status === 0 ||
        (event.detail.status === 403 && !appKioskMode)) {
      return;  // No internet connectivity or not signed in.
    }
    this.cloudPrintErrorMessage_ = event.detail.message;
    this.error_ = Error.CLOUD_PRINT_ERROR;
    this.$.state.transitTo(State.FATAL_ERROR);
    if (event.detail.status === 200) {
      console.warn(
          'Google Cloud Print Error: ' +
          `(${event.detail.errorCode}) ${event.detail.message}`);
    } else {
      console.warn(
          'Google Cloud Print Error: ' +
          `HTTP status ${event.detail.status}`);
    }
  },

  /**
   * Updates printing options according to source document presets.
   * @param {boolean} disableScaling Whether the document disables scaling.
   * @param {number} copies The default number of copies from the document.
   * @param {!DuplexMode} duplex The default duplex setting
   *     from the document.
   * @private
   */
  onPrintPresetOptions_(disableScaling, copies, duplex) {
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
  },

  /**
   * @param {!CustomEvent<number>} e Contains the new preview request ID.
   * @private
   */
  onPreviewStart_(e) {
    this.$.documentInfo.inFlightRequestId = e.detail;
  },

  /** @private */
  close_() {
    this.$.state.transitTo(State.CLOSING);
  },
});
