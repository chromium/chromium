// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * Number of settings sections to show when "More settings" is collapsed.
 * @type {number}
 */
const MAX_SECTIONS_TO_SHOW = 6;

Polymer({
  is: 'print-preview-app',

  behaviors: [SettingsBehavior, CrContainerShadowBehavior],

  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * @type {!Object}
     */
    settings: {
      type: Object,
      notify: true,
    },

    /** @private {print_preview.Destination} */
    destination_: {
      type: Object,
      notify: true,
    },

    /** @private {?print_preview.DestinationStore} */
    destinationStore_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {print_preview.DocumentInfo} */
    documentInfo_: {
      type: Object,
      notify: true,
    },

    /** @private {?print_preview.InvitationStore} */
    invitationStore_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {!Array<print_preview.RecentDestination>} */
    recentDestinations_: {
      type: Array,
      notify: true,
    },

    /** @type {!print_preview_new.State} */
    state: {
      type: Number,
      observer: 'onStateChanged_',
    },

    /** @private {?print_preview.UserInfo} */
    userInfo_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {string} */
    errorMessage_: {
      type: String,
      notify: true,
      value: '',
    },

    /** @private {boolean} */
    controlsDisabled_: {
      type: Boolean,
      notify: true,
      computed: 'computeControlsDisabled_(state)',
    },

    /** @private {?print_preview.MeasurementSystem} */
    measurementSystem_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {boolean} */
    isInAppKioskMode_: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /** @private {!print_preview_new.PreviewAreaState} */
    previewState_: {
      type: String,
      observer: 'onPreviewAreaStateChanged_',
    },

    /** @private {boolean} */
    settingsExpandedByUser_: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /** @private {boolean} */
    shouldShowMoreSettings_: {
      type: Boolean,
      notify: true,
      computed: 'computeShouldShowMoreSettings_(settings.pages.available, ' +
          'settings.copies.available, settings.layout.available, ' +
          'settings.color.available, settings.mediaSize.available, ' +
          'settings.dpi.available, settings.margins.available, ' +
          'settings.pagesPerSheet.available, settings.scaling.available, ' +
          'settings.otherOptions.available, settings.vendorItems.available)',
    },

    /**
     * Whether to show pages per sheet feature or not.
     * @private {boolean}
     */
    showPagesPerSheet_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('pagesPerSheetEnabled');
      },
    },
  },

  listeners: {
    'cr-dialog-open': 'onCrDialogOpen_',
    'close': 'onCrDialogClose_',
  },

  /** @private {?WebUIListenerTracker} */
  listenerTracker_: null,

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {?cloudprint.CloudPrintInterface} */
  cloudPrintInterface_: null,

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private {boolean} */
  cancelled_: false,

  /** @private {boolean} */
  showSystemDialogBeforePrint_: false,

  /** @private {boolean} */
  openPdfInPreview_: false,

  /** @private {boolean} */
  isInKioskAutoPrintMode_: false,

  /** @private {!Array<!CrDialogElement>} */
  openDialogs_: [],

  /** @override */
  ready: function() {
    cr.ui.FocusOutlineManager.forDocument(document);
  },

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();
    this.documentInfo_ = new print_preview.DocumentInfo();
    this.userInfo_ = new print_preview.UserInfo();
    this.listenerTracker_ = new WebUIListenerTracker();
    this.listenerTracker_.add(
        'use-cloud-print', this.onCloudPrintEnable_.bind(this));
    this.listenerTracker_.add('print-failed', this.onPrintFailed_.bind(this));
    this.listenerTracker_.add(
        'print-preset-options', this.onPrintPresetOptions_.bind(this));
    this.destinationStore_ = new print_preview.DestinationStore(
        this.userInfo_, this.listenerTracker_);
    this.invitationStore_ = new print_preview.InvitationStore(this.userInfo_);
    this.tracker_.add(window, 'keydown', this.onKeyDown_.bind(this));
    this.$.previewArea.setPluginKeyEventCallback(this.onKeyDown_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType.DESTINATION_SELECT,
        this.onDestinationSelect_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType
            .SELECTED_DESTINATION_CAPABILITIES_READY,
        this.onDestinationUpdated_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType
            .SELECTED_DESTINATION_UNSUPPORTED,
        this.onInvalidPrinter_.bind(this));
    this.nativeLayer_.getInitialSettings().then(
        this.onInitialSettingsSet_.bind(this));
  },

  /** @override */
  detached: function() {
    this.listenerTracker_.removeAll();
    this.tracker_.removeAll();
  },

  /**
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  computeControlsDisabled_: function() {
    return this.state != print_preview_new.State.READY;
  },

  /**
   * Consume escape and enter key presses and ctrl + shift + p. Delegate
   * everything else to the preview area.
   * @param {!KeyboardEvent} e The keyboard event.
   * @private
   */
  onKeyDown_: function(e) {
    // Escape key closes the topmost dialog that is currently open within
    // Print Preview. If no such dialog exists, then the Print Preview dialog
    // itself is closed.
    if (e.code == 'Escape' && !hasKeyModifiers(e)) {
      // Don't close the Print Preview dialog if there is a child dialog open.
      if (this.openDialogs_.length != 0) {
        // Manually cancel the dialog, since we call preventDefault() to prevent
        // views from closing the Print Preview dialog.
        const dialogToClose = this.openDialogs_[this.openDialogs_.length - 1];
        dialogToClose.cancel();
        e.preventDefault();
        return;
      }

      // On non-mac with toolkit-views, ESC key is handled by C++-side instead
      // of JS-side.
      if (cr.isMac) {
        this.close_();
        e.preventDefault();
      }
      return;
    }

    // On Mac, Cmd+Period should close the print dialog.
    if (cr.isMac && e.code == 'Period' && e.metaKey) {
      this.close_();
      e.preventDefault();
      return;
    }

    // Ctrl + Shift + p / Mac equivalent.
    if (e.code == 'KeyP') {
      if ((cr.isMac && e.metaKey && e.altKey && !e.shiftKey && !e.ctrlKey) ||
          (!cr.isMac && e.shiftKey && e.ctrlKey && !e.altKey && !e.metaKey)) {
        // Don't use system dialog if the link isn't available.
        const linkContainer = this.$$('print-preview-link-container');
        if (!linkContainer || !linkContainer.systemDialogLinkAvailable()) {
          e.preventDefault();
          return;
        }

        // Don't try to print with system dialog on Windows if the document is
        // not ready, because we send the preview document to the printer on
        // Windows.
        if (!cr.isWindows || this.state == print_preview_new.State.READY)
          this.onPrintWithSystemDialog_();
        e.preventDefault();
        return;
      }
    }

    if ((e.code === 'Enter' || e.code === 'NumpadEnter') &&
        this.state === print_preview_new.State.READY &&
        this.openDialogs_.length === 0) {
      const activeElementTag = e.path[0].tagName;
      if (['PAPER-BUTTON', 'BUTTON', 'SELECT', 'A', 'CR-CHECKBOX'].includes(
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
  onCrDialogOpen_: function(e) {
    this.openDialogs_.push(
        /** @type {!CrDialogElement} */ (e.composedPath()[0]));
  },

  /**
   * @param {!Event} e The close event.
   * @private
   */
  onCrDialogClose_: function(e) {
    // Note: due to event re-firing in cr_dialog.js, this event will always
    // appear to be coming from the outermost child dialog.
    // TODO(rbpotter): Fix event re-firing so that the event comes from the
    // dialog that has been closed, and add an assertion that the removed
    // dialog matches e.composedPath()[0].
    if (e.composedPath()[0].nodeName == 'CR-DIALOG')
      this.openDialogs_.pop();
  },

  /**
   * @param {!print_preview.NativeInitialSettings} settings
   * @private
   */
  onInitialSettingsSet_: function(settings) {
    this.documentInfo_.init(
        settings.previewModifiable, settings.documentTitle,
        settings.documentHasSelection);
    this.notifyPath('documentInfo_.isModifiable');
    this.notifyPath('documentInfo_.hasSelection');
    this.notifyPath('documentInfo_.title');
    this.notifyPath('documentInfo_.pageCount');
    this.$.model.setStickySettings(settings.serializedAppStateStr);
    this.$.model.setPolicySettings(
        settings.headerFooter, settings.isHeaderFooterManaged);
    this.measurementSystem_ = new print_preview.MeasurementSystem(
        settings.thousandsDelimeter, settings.decimalDelimeter,
        settings.unitType);
    this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
    this.destinationStore_.init(
        settings.isInAppKioskMode, settings.printerName,
        settings.serializedDefaultDestinationSelectionRulesStr,
        this.recentDestinations_);
    this.isInAppKioskMode_ = settings.isInAppKioskMode;
    this.isInKioskAutoPrintMode_ = settings.isInKioskAutoPrintMode;

    // This is only visible in the task manager.
    let title = document.head.querySelector('title');
    if (!title) {
      title = document.createElement('title');
      document.head.appendChild(title);
    }
    title.textContent = settings.documentTitle;
  },

  /**
   * Called when Google Cloud Print integration is enabled by the
   * PrintPreviewHandler.
   * Fetches the user's cloud printers.
   * @param {string} cloudPrintUrl The URL to use for cloud print servers.
   * @param {boolean} appKioskMode Whether to print automatically for kiosk
   *     mode.
   * @private
   */
  onCloudPrintEnable_: function(cloudPrintUrl, appKioskMode) {
    assert(!this.cloudPrintInterface_);
    this.cloudPrintInterface_ = new cloudprint.CloudPrintInterface(
        cloudPrintUrl, assert(this.nativeLayer_), assert(this.userInfo_),
        appKioskMode);
    this.tracker_.add(
        assert(this.cloudPrintInterface_),
        cloudprint.CloudPrintInterfaceEventType.SUBMIT_DONE,
        this.close_.bind(this));
    [cloudprint.CloudPrintInterfaceEventType.SEARCH_FAILED,
     cloudprint.CloudPrintInterfaceEventType.SUBMIT_FAILED,
     cloudprint.CloudPrintInterfaceEventType.PRINTER_FAILED,
    ].forEach(eventType => {
      this.tracker_.add(
          assert(this.cloudPrintInterface_), eventType,
          this.onCloudPrintError_.bind(this));
    });

    this.destinationStore_.setCloudPrintInterface(this.cloudPrintInterface_);
    this.invitationStore_.setCloudPrintInterface(this.cloudPrintInterface_);
    if (this.$.destinationSettings.isDialogOpen()) {
      this.destinationStore_.startLoadCloudDestinations();
      this.invitationStore_.startLoadingInvitations();
    }
  },

  /** @private */
  onDestinationSelect_: function() {
    // If the plugin does not exist do not attempt to load the preview.
    if (this.state == print_preview_new.State.FATAL_ERROR)
      return;

    this.$.state.transitTo(print_preview_new.State.NOT_READY);
    this.destination_ = this.destinationStore_.selectedDestination;
  },

  /** @private */
  onDestinationUpdated_: function() {
    // Notify observers, since destination_ ==
    // destinationStore_.selectedDestination and this event indicates
    // destinationStore_.selectedDestination.capabilities has been updated.
    this.notifyPath('destination_.capabilities');

    if (!this.$.model.initialized())
      this.$.model.applyStickySettings();

    if (this.state == print_preview_new.State.NOT_READY ||
        this.state == print_preview_new.State.INVALID_PRINTER) {
      this.$.state.transitTo(print_preview_new.State.READY);
      if (this.isInKioskAutoPrintMode_)
        this.onPrintRequested_();
    }
  },

  /**
   * @param {!CustomEvent} e Event containing the sticky settings string.
   * @private
   */
  onSaveStickySettings_: function(e) {
    this.nativeLayer_.saveAppState(/** @type {string} */ (e.detail));
  },

  /** @private */
  onStateChanged_: function() {
    if (this.state == print_preview_new.State.CLOSING) {
      this.remove();
      this.nativeLayer_.dialogClose(this.cancelled_);
    } else if (this.state == print_preview_new.State.HIDDEN) {
      if (this.destination_.isLocal &&
          this.destination_.id !==
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
        // Only hide the preview for local, non PDF destinations.
        this.nativeLayer_.hidePreview();
      }
    } else if (this.state == print_preview_new.State.PRINTING) {
      if (this.shouldShowMoreSettings_) {
        new print_preview.PrintSettingsUiMetricsContext().record(
            this.settingsExpandedByUser_ ?
                print_preview.Metrics.PrintSettingsUiBucket
                    .PRINT_WITH_SETTINGS_EXPANDED :
                print_preview.Metrics.PrintSettingsUiBucket
                    .PRINT_WITH_SETTINGS_COLLAPSED);
      }
      const destination = assert(this.destinationStore_.selectedDestination);
      const whenPrintDone =
          this.nativeLayer_.print(this.$.model.createPrintTicket(
              destination, this.openPdfInPreview_,
              this.showSystemDialogBeforePrint_));
      if (destination.isLocal) {
        const onError = destination.id ==
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ?
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
  onPrintRequested_: function() {
    this.$.state.transitTo(
        this.$.previewArea.previewLoaded() ? print_preview_new.State.PRINTING :
                                             print_preview_new.State.HIDDEN);
  },

  /** @private */
  onCancelRequested_: function() {
    this.cancelled_ = true;
    this.$.state.transitTo(print_preview_new.State.CLOSING);
  },

  /**
   * @param {!CustomEvent} e The event containing the new validity.
   * @private
   */
  onSettingValidChanged_: function(e) {
    this.$.state.transitTo(
        /** @type {boolean} */ (e.detail) ?
            print_preview_new.State.READY :
            print_preview_new.State.INVALID_TICKET);
  },

  /** @private */
  onFileSelectionCancel_: function() {
    this.$.state.transitTo(print_preview_new.State.READY);
  },

  /**
   * Called when the native layer has retrieved the data to print to Google
   * Cloud Print.
   * @param {string} data The body to send in the HTTP request.
   * @private
   */
  onPrintToCloud_: function(data) {
    assert(
        this.cloudPrintInterface_ != null, 'Google Cloud Print is not enabled');
    const destination = assert(this.destinationStore_.selectedDestination);
    this.cloudPrintInterface_.submit(
        destination, this.$.model.createCloudJobTicket(destination),
        assert(this.documentInfo_), data);
  },

  // <if expr="not chromeos">
  /** @private */
  onPrintWithSystemDialog_: function() {
    assert(!cr.isChromeOS);
    if (cr.isWindows) {
      this.showSystemDialogBeforePrint_ = true;
      this.onPrintRequested_();
      return;
    }
    this.nativeLayer_.showSystemDialog();
    this.$.state.transitTo(print_preview_new.State.SYSTEM_DIALOG);
  },
  // </if>

  // <if expr="is_macosx">
  /** @private */
  onOpenPdfInPreview_: function() {
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
  onPrintFailed_: function(httpError) {
    console.error('Printing failed with error code ' + httpError);
    this.errorMessage_ = httpError.toString();
    this.$.state.transitTo(print_preview_new.State.FATAL_ERROR);
  },

  /** @private */
  onInvalidPrinter_: function() {
    this.previewState_ =
        print_preview_new.PreviewAreaState.UNSUPPORTED_CLOUD_PRINTER;
  },

  /** @private */
  onPreviewAreaStateChanged_: function() {
    switch (this.previewState_) {
      case print_preview_new.PreviewAreaState.PREVIEW_FAILED:
      case print_preview_new.PreviewAreaState.NO_PLUGIN:
        this.$.state.transitTo(print_preview_new.State.FATAL_ERROR);
        break;
      case print_preview_new.PreviewAreaState.INVALID_SETTINGS:
      case print_preview_new.PreviewAreaState.UNSUPPORTED_CLOUD_PRINTER:
        if (this.state != print_preview_new.State.INVALID_PRINTER)
          this.$.state.transitTo(print_preview_new.State.INVALID_PRINTER);
        break;
      case print_preview_new.PreviewAreaState.DISPLAY_PREVIEW:
      case print_preview_new.PreviewAreaState.OPEN_IN_PREVIEW_LOADED:
        if (this.state == print_preview_new.State.HIDDEN)
          this.$.state.transitTo(print_preview_new.State.PRINTING);
        break;
      default:
        break;
    }
  },

  /**
   * Called when there was an error communicating with Google Cloud print.
   * Displays an error message in the print header.
   * @param {!Event} event Contains the error message.
   * @private
   */
  onCloudPrintError_: function(event) {
    if (event.status == 0) {
      return;  // Ignore, the system does not have internet connectivity.
    }
    if (event.status == 403) {
      if (!this.isInAppKioskMode_) {
        this.$.destinationSettings.showCloudPrintPromo();
      }
    } else {
      this.set('state_.cloudPrintError', event.message);
    }
    if (event.status == 200) {
      console.error(
          `Google Cloud Print Error: (${event.errorCode}) ${event.message}`);
    } else {
      console.error(`Google Cloud Print Error: HTTP status ${event.status}`);
    }
  },

  /**
   * Updates printing options according to source document presets.
   * @param {boolean} disableScaling Whether the document disables scaling.
   * @param {number} copies The default number of copies from the document.
   * @param {!print_preview_new.DuplexMode} duplex The default duplex setting
   *     from the document.
   * @private
   */
  onPrintPresetOptions_: function(disableScaling, copies, duplex) {
    if (disableScaling)
      this.documentInfo_.updateIsScalingDisabled(true);

    if (copies > 0 && this.getSetting('copies').available)
      this.setSetting('copies', copies);

    if (duplex !== print_preview_new.DuplexMode.UNKNOWN_DUPLEX_MODE &&
        this.getSetting('duplex').available) {
      this.setSetting(
          'duplex', duplex === print_preview_new.DuplexMode.LONG_EDGE);
    }
  },

  /**
   * @return {boolean} Whether to show the "More settings" link.
   * @private
   */
  computeShouldShowMoreSettings_: function() {
    // Destination settings is always available. See if the total number of
    // available sections exceeds the maximum number to show.
    return [
      'pages', 'copies', 'layout', 'color', 'mediaSize', 'margins', 'color',
      'pagesPerSheet', 'scaling', 'otherOptions', 'vendorItems'
    ].reduce((count, setting) => {
      return this.getSetting(setting).available ? count + 1 : count;
    }, 1) > MAX_SECTIONS_TO_SHOW;
  },

  /**
   * @return {boolean} Whether the "more settings" collapse should be expanded.
   * @private
   */
  shouldExpandSettings_: function() {
    if (this.settingsExpandedByUser_ === undefined ||
        this.shouldShowMoreSettings_ === undefined) {
      return false;
    }

    // Expand the settings if the user has requested them expanded or if more
    // settings is not displayed (i.e. less than 6 total settings available).
    return this.settingsExpandedByUser_ || !this.shouldShowMoreSettings_;
  },

  /** @private */
  close_: function() {
    this.$.state.transitTo(print_preview_new.State.CLOSING);
  },
});
})();
