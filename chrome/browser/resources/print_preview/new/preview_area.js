// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

/**
 * @typedef {{
 *   width_microns: number,
 *   height_microns: number,
 * }}
 */
print_preview_new.MediaSizeValue;

/** @enum {string} */
print_preview_new.PreviewAreaState = {
  NO_PLUGIN: 'no-plugin',
  LOADING: 'loading',
  DISPLAY_PREVIEW: 'display-preview',
  OPEN_IN_PREVIEW_LOADING: 'open-in-preview-loading',
  OPEN_IN_PREVIEW_LOADED: 'open-in-preview-loaded',
  INVALID_SETTINGS: 'invalid-settings',
  PREVIEW_FAILED: 'preview-failed',
  UNSUPPORTED_CLOUD_PRINTER: 'unsupported-cloud-printer',
};

Polymer({
  is: 'print-preview-preview-area',

  behaviors: [WebUIListenerBehavior, SettingsBehavior, I18nBehavior],

  properties: {
    /** @type {print_preview.DocumentInfo} */
    documentInfo: Object,

    /** @type {print_preview.Destination} */
    destination: Object,

    /** @type {!print_preview_new.State} */
    state: {
      type: Number,
      observer: 'onStateChanged_',
    },

    /** @type {?print_preview.MeasurementSystem} */
    measurementSystem: Object,

    /** @private {boolean} Whether the plugin is loaded */
    pluginLoaded_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} Whether the document is ready */
    documentReady_: {
      type: Boolean,
      value: false,
    },

    /** @type {!print_preview_new.PreviewAreaState} */
    previewState: {
      type: String,
      notify: true,
      value: print_preview_new.PreviewAreaState.LOADING,
    },

    /** @private {boolean} */
    previewLoaded_: {
      type: Boolean,
      notify: true,
      computed: 'computePreviewLoaded_(documentReady_, pluginLoaded_)',
    },
  },

  listeners: {
    'pointerover': 'onPointerOver_',
    'pointerout': 'onPointerOut_',
  },

  observers: [
    'onSettingsChanged_(settings.color.value, settings.cssBackground.value, ' +
        'settings.fitToPage.value, settings.headerFooter.value, ' +
        'settings.layout.value, settings.ranges.value, ' +
        'settings.selectionOnly.value, settings.scaling.value, ' +
        'settings.rasterize.value, destination)',
    'onMarginsChanged_(settings.margins.value)',
    'onCustomMarginsChanged_(settings.customMargins.value)',
    'onMediaSizeChanged_(settings.mediaSize.value)',
    'onPagesPerSheetChanged_(settings.pagesPerSheet.value)',
    'pluginOrDocumentStatusChanged_(pluginLoaded_, documentReady_)',
  ],

  /** @private {?print_preview.NativeLayer} */
  nativeLayer_: null,

  /**
   * @private {?print_preview.MarginsSetting}
   */
  lastCustomMargins_: null,

  /**
   * @private {?print_preview_new.MediaSizeValue}
   */
  lastMediaSize_: null,

  /** @private {number} */
  inFlightRequestId_: -1,

  /** @private {boolean} */
  requestPreviewWhenReady_: false,

  /** @private {?print_preview_new.PluginProxy} */
  pluginProxy_: null,

  /** @private {?function(!KeyboardEvent)} */
  keyEventCallback_: null,

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();
    this.addWebUIListener(
        'page-count-ready', this.onPageCountReady_.bind(this));
    this.addWebUIListener(
        'page-layout-ready', this.onPageLayoutReady_.bind(this));
    this.addWebUIListener(
        'page-preview-ready', this.onPagePreviewReady_.bind(this));

    this.pluginProxy_ = print_preview_new.PluginProxy.getInstance();
    if (!this.pluginProxy_.checkPluginCompatibility(assert(
            this.$$('.preview-area-compatibility-object-out-of-process')))) {
      this.previewState = print_preview_new.PreviewAreaState.NO_PLUGIN;
    }
  },


  /**
   * @return {boolean} Whether the preview is loaded.
   * @private
   */
  computePreviewLoaded_: function() {
    return this.documentReady_ && this.pluginLoaded_;
  },

  /** @return {boolean} Whether the preview is loaded. */
  previewLoaded: function() {
    return this.previewLoaded_;
  },

  /**
   * Called when the pointer moves onto the component. Shows the margin
   * controls if custom margins are being used.
   * @param {!Event} event Contains element pointer moved from.
   * @private
   */
  onPointerOver_: function(event) {
    const marginControlContainer = this.$.marginControlContainer;
    let fromElement = event.fromElement;
    while (fromElement != null) {
      if (fromElement == marginControlContainer)
        return;

      fromElement = fromElement.parentElement;
    }
    marginControlContainer.setInvisible(false);
  },

  /**
   * Called when the pointer moves off of the component. Hides the margin
   * controls if they are visible.
   * @param {!Event} event Contains element pointer moved to.
   * @private
   */
  onPointerOut_: function(event) {
    const marginControlContainer = this.$.marginControlContainer;
    let toElement = event.toElement;
    while (toElement != null) {
      if (toElement == marginControlContainer)
        return;

      toElement = toElement.parentElement;
    }
    marginControlContainer.setInvisible(true);
  },

  /** @private */
  onSettingsChanged_: function() {
    if (this.state == print_preview_new.State.READY) {
      this.startPreview_();
      return;
    }
    this.requestPreviewWhenReady_ = true;
  },

  /** @private */
  pluginOrDocumentStatusChanged_: function() {
    if (!this.pluginLoaded_ || !this.documentReady_)
      return;

    this.previewState = this.previewState ==
            print_preview_new.PreviewAreaState.OPEN_IN_PREVIEW_LOADING ?
        print_preview_new.PreviewAreaState.OPEN_IN_PREVIEW_LOADED :
        print_preview_new.PreviewAreaState.DISPLAY_PREVIEW;
  },

  /**
   * @return {string} 'invisible' if overlay is invisible, '' otherwise.
   * @private
   */
  getInvisible_: function() {
    return this.isInDisplayPreviewState_() ? 'invisible' : '';
  },

  /**
   * @return {string} 'true' if overlay is aria-hidden, 'false' otherwise.
   * @private
   */
  getAriaHidden_: function() {
    return this.isInDisplayPreviewState_().toString();
  },

  /**
   * @return {boolean} Whether the preview area is in DISPLAY_PREVIEW state.
   * @private
   */
  isInDisplayPreviewState_: function() {
    return this.previewState ==
        print_preview_new.PreviewAreaState.DISPLAY_PREVIEW;
  },

  /**
   * @return {boolean} Whether the preview is currently loading.
   * @private
   */
  isPreviewLoading_: function() {
    return this.previewState == print_preview_new.PreviewAreaState.LOADING;
  },

  /**
   * @return {string} 'jumping-dots' to enable animation, '' otherwise.
   * @private
   */
  getJumpingDots_: function() {
    return this.isPreviewLoading_() ? 'jumping-dots' : '';
  },

  /**
   * @return {boolean} Whether the "learn more" link to the cloud print help
   *     page should be shown.
   * @private
   */
  shouldShowLearnMoreLink_: function() {
    return this.previewState ==
        print_preview_new.PreviewAreaState.UNSUPPORTED_CLOUD_PRINTER;
  },

  /**
   * @return {string} The current preview area message to display.
   * @private
   */
  currentMessage_: function() {
    switch (this.previewState) {
      case print_preview_new.PreviewAreaState.NO_PLUGIN:
        return this.i18nAdvanced('noPlugin');
      case print_preview_new.PreviewAreaState.LOADING:
        return this.i18nAdvanced('loading');
      case print_preview_new.PreviewAreaState.DISPLAY_PREVIEW:
        return '';
      // <if expr="is_macosx">
      case print_preview_new.PreviewAreaState.OPEN_IN_PREVIEW_LOADING:
      case print_preview_new.PreviewAreaState.OPEN_IN_PREVIEW_LOADED:
        return this.i18nAdvanced('openingPDFInPreview');
      // </if>
      case print_preview_new.PreviewAreaState.INVALID_SETTINGS:
        return this.i18nAdvanced('invalidPrinterSettings', {
          substitutions: [],
          tags: ['BR'],
        });
      case print_preview_new.PreviewAreaState.PREVIEW_FAILED:
        return this.i18nAdvanced('previewFailed');
      case print_preview_new.PreviewAreaState.UNSUPPORTED_CLOUD_PRINTER:
        return this.i18nAdvanced('unsupportedCloudPrinter', {
          substitutions: [],
          tags: ['BR'],
        });
      default:
        return '';
    }
  },

  /** @private */
  startPreview_: function() {
    this.previewState = print_preview_new.PreviewAreaState.LOADING;
    this.documentReady_ = false;
    this.getPreview_().then(
        previewUid => {
          if (!this.documentInfo.isModifiable)
            this.onPreviewStart_(previewUid, -1);
          this.documentReady_ = true;
        },
        type => {
          if (/** @type{string} */ (type) == 'SETTINGS_INVALID') {
            this.previewState =
                print_preview_new.PreviewAreaState.INVALID_SETTINGS;
          } else if (/** @type{string} */ (type) != 'CANCELLED') {
            this.previewState =
                print_preview_new.PreviewAreaState.PREVIEW_FAILED;
          }
        });
  },

  /** @private */
  onStateChanged_: function() {
    if (this.state == print_preview_new.State.READY &&
        this.requestPreviewWhenReady_) {
      this.startPreview_();
      this.requestPreviewWhenReady_ = false;
    }
  },

  // <if expr="is_macosx">
  /** Set the preview state to display the "opening in preview" message. */
  setOpeningPdfInPreview: function() {
    assert(cr.isMac);
    this.previewState =
        this.previewState == print_preview_new.PreviewAreaState.LOADING ?
        print_preview_new.PreviewAreaState.OPEN_IN_PREVIEW_LOADING :
        print_preview_new.PreviewAreaState.OPEN_IN_PREVIEW_LOADED;
  },
  // </if>

  /**
   * @param {number} previewUid The unique identifier of the preview.
   * @param {number} index The index of the page to preview.
   * @private
   */
  onPreviewStart_: function(previewUid, index) {
    if (!this.pluginProxy_.pluginReady()) {
      const plugin = this.pluginProxy_.createPlugin(previewUid, index);
      plugin.setKeyEventCallback(this.keyEventCallback_);
      this.$$('.preview-area-plugin-wrapper')
          .appendChild(
              /** @type {Node} */ (plugin));
      plugin.setLoadCallback(this.onPluginLoad_.bind(this));
      plugin.setViewportChangedCallback(
          this.onPreviewVisualStateChange_.bind(this));
    }

    this.pluginLoaded_ = false;
    this.pluginProxy_.resetPrintPreviewMode(
        previewUid, index, !this.getSettingValue('color'),
        /** @type {!Array<number>} */ (this.getSettingValue('pages')),
        this.documentInfo.isModifiable);
  },

  /**
   * Called when the page layout of the document is ready. Always occurs
   * as a result of a preview request.
   * @param {{marginTop: number,
   *          marginLeft: number,
   *          marginBottom: number,
   *          marginRight: number,
   *          contentWidth: number,
   *          contentHeight: number,
   *          printableAreaX: number,
   *          printableAreaY: number,
   *          printableAreaWidth: number,
   *          printableAreaHeight: number,
   *        }} pageLayout Layout information about the document.
   * @param {boolean} hasCustomPageSizeStyle Whether this document has a
   *     custom page size or style to use.
   * @private
   */
  onPageLayoutReady_: function(pageLayout, hasCustomPageSizeStyle) {
    const origin = new print_preview.Coordinate2d(
        pageLayout.printableAreaX, pageLayout.printableAreaY);
    const size = new print_preview.Size(
        pageLayout.printableAreaWidth, pageLayout.printableAreaHeight);

    const margins = new print_preview.Margins(
        Math.round(pageLayout.marginTop), Math.round(pageLayout.marginRight),
        Math.round(pageLayout.marginBottom), Math.round(pageLayout.marginLeft));

    const o = print_preview.ticket_items.CustomMarginsOrientation;
    const pageSize = new print_preview.Size(
        pageLayout.contentWidth + margins.get(o.LEFT) + margins.get(o.RIGHT),
        pageLayout.contentHeight + margins.get(o.TOP) + margins.get(o.BOTTOM));

    this.documentInfo.updatePageInfo(
        new print_preview.PrintableArea(origin, size), pageSize,
        hasCustomPageSizeStyle, margins);
    this.notifyPath('documentInfo.printableArea');
    this.notifyPath('documentInfo.pageSize');
    this.notifyPath('documentInfo.margins');
    this.notifyPath('documentInfo.hasCssMediaStyles');
  },

  /**
   * Called when the document page count is received from the native layer.
   * Always occurs as a result of a preview request.
   * @param {number} pageCount The document's page count.
   * @param {number} previewResponseId The request ID for this page count event.
   * @param {number} fitToPageScaling The scaling required to fit the document
   *     to page.
   * @private
   */
  onPageCountReady_: function(pageCount, previewResponseId, fitToPageScaling) {
    if (this.inFlightRequestId_ != previewResponseId)
      return;
    this.documentInfo.updatePageCount(pageCount);
    this.documentInfo.updateFitToPageScaling(fitToPageScaling);
    this.notifyPath('documentInfo.pageCount');
    this.notifyPath('documentInfo.fitToPageScaling');
  },

  /**
   * Called when the plugin loads. This is a consequence of calling
   * plugin.reload(). Certain plugin state can only be set after the plugin
   * has loaded.
   * @param {boolean} success Whether the plugin load succeeded or not.
   * @private
   */
  onPluginLoad_: function(success) {
    if (success) {
      this.pluginLoaded_ = true;
    } else {
      this.previewState = print_preview_new.PreviewAreaState.PREVIEW_FAILED;
    }
  },

  /**
   * Called when the preview plugin's visual state has changed. This is a
   * consequence of scrolling or zooming the plugin. Updates the custom
   * margins component if shown.
   * @param {number} pageX The horizontal offset for the page corner in pixels.
   * @param {number} pageY The vertical offset for the page corner in pixels.
   * @param {number} pageWidth The page width in pixels.
   * @param {number} viewportWidth The viewport width in pixels.
   * @param {number} viewportHeight The viewport height in pixels.
   * @private
   */
  onPreviewVisualStateChange_: function(
      pageX, pageY, pageWidth, viewportWidth, viewportHeight) {
    this.$.marginControlContainer.updateTranslationTransform(
        new print_preview.Coordinate2d(pageX, pageY));
    this.$.marginControlContainer.updateScaleTransform(
        pageWidth / this.documentInfo.pageSize.width);
    this.$.marginControlContainer.updateClippingMask(
        new print_preview.Size(viewportWidth, viewportHeight));
  },

  /**
   * Called when a page's preview has been generated.
   * @param {number} pageIndex The index of the page whose preview is ready.
   * @param {number} previewUid The unique ID of the print preview UI.
   * @param {number} previewResponseId The preview request ID that this page
   *     preview is a response to.
   * @private
   */
  onPagePreviewReady_: function(pageIndex, previewUid, previewResponseId) {
    if (this.inFlightRequestId_ != previewResponseId)
      return;
    const pageNumber = pageIndex + 1;
    let index = this.getSettingValue('pages').indexOf(pageNumber);
    // When pagesPerSheet > 1, the backend will always return page indices 0 to
    // N-1, where N is the total page count of the N-upped document.
    const pagesPerSheet =
        /** @type {number} */ (this.getSettingValue('pagesPerSheet'));
    if (pagesPerSheet > 1)
      index = pageIndex;
    if (index == 0)
      this.onPreviewStart_(previewUid, pageIndex);
    if (index != -1)
      this.pluginProxy_.loadPreviewPage(previewUid, pageIndex, index);
  },

  /**
   * Processes a keyboard event that could possibly be used to change state of
   * the preview plugin.
   * @param {!KeyboardEvent} e Keyboard event to process.
   */
  handleDirectionalKeyEvent: function(e) {
    // Make sure the PDF plugin is there.
    // We only care about: PageUp, PageDown, Left, Up, Right, Down.
    // If the user is holding a modifier key, ignore.
    if (!this.pluginProxy_.pluginReady() ||
        !['PageUp', 'PageDown', 'ArrowLeft', 'ArrowRight', 'ArrowUp',
          'ArrowDown']
             .includes(e.code) ||
        hasKeyModifiers(e)) {
      return;
    }

    // Don't handle the key event for these elements.
    const tagName = e.composedPath()[0].tagName;
    if (['INPUT', 'SELECT', 'EMBED'].includes(tagName))
      return;

    // For the most part, if any div of header was the last clicked element,
    // then the active element is the body. Starting with the last clicked
    // element, and work up the DOM tree to see if any element has a
    // scrollbar. If there exists a scrollbar, do not handle the key event
    // here.
    const isEventHorizontal = ['ArrowLeft', 'ArrowRight'].includes(e.code);
    for (let i = 0; i < e.composedPath().length; i++) {
      const element = e.composedPath()[i];
      if (element.scrollHeight > element.clientHeight && !isEventHorizontal ||
          element.scrollWidth > element.clientWidth && isEventHorizontal) {
        return;
      }
    }

    // No scroll bar anywhere, or the active element is something else, like a
    // button. Note: buttons have a bigger scrollHeight than clientHeight.
    this.pluginProxy_.sendKeyEvent(e);
    e.preventDefault();
  },

  /**
   * Set a callback that gets called when a key event is received that
   * originates in the plugin.
   * @param {function(KeyboardEvent)} callback The callback to be called with
   *     a key event.
   */
  setPluginKeyEventCallback: function(callback) {
    this.keyEventCallback_ = callback;
  },

  /**
   * Called when dragging margins starts or stops.
   */
  onMarginDragChanged_: function(e) {
    if (!this.pluginProxy_.pluginReady())
      return;

    // When hovering over the plugin (which may be in a separate iframe)
    // pointer events will be sent to the frame. When dragging the margins,
    // we don't want this to happen as it can cause the margin to stop
    // being draggable.
    this.pluginProxy_.setPointerEvents(!e.detail);
  },

  /** @private */
  onMarginsChanged_: function() {
    if (this.getSettingValue('margins') !=
        print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      this.onSettingsChanged_();
    } else {
      const customMargins =
          /** @type {!print_preview.MarginsSetting} */ (
              this.getSettingValue('customMargins'));

      for (let side of Object.values(
               print_preview.ticket_items.CustomMarginsOrientation)) {
        const key = print_preview_new.MARGIN_KEY_MAP.get(side);
        // If custom margins are undefined, return and wait for them to be set.
        if (customMargins[key] === undefined || !this.documentInfo ||
            !this.documentInfo.margins) {
          return;
        }

        // Start a preview request if the margins actually changed.
        if (this.documentInfo.margins.get(side) != customMargins[key]) {
          this.onSettingsChanged_();
          break;
        }
      }
      this.lastCustomMargins_ = customMargins;
    }
  },

  /** @private */
  onCustomMarginsChanged_: function() {
    const newValue =
        /** @type {!print_preview.MarginsSetting} */ (
            this.getSettingValue('customMargins'));
    if (!!this.lastCustomMargins_ &&
        this.lastCustomMargins_.marginTop !== undefined &&
        this.getSettingValue('margins') ==
            print_preview.ticket_items.MarginsTypeValue.CUSTOM &&
        (this.lastCustomMargins_.marginTop != newValue.marginTop ||
         this.lastCustomMargins_.marginLeft != newValue.marginLeft ||
         this.lastCustomMargins_.marginRight != newValue.marginRight ||
         this.lastCustomMargins_.marginBottom != newValue.marginBottom)) {
      this.onSettingsChanged_();
    }
    this.lastCustomMargins_ = newValue;
  },

  /** @private */
  onMediaSizeChanged_: function() {
    const newValue =
        /** @type {!print_preview_new.MediaSizeValue} */ (
            this.getSettingValue('mediaSize'));
    if (!!this.lastMediaSize_ &&
        (newValue.height_microns != this.lastMediaSize_.height_microns ||
         newValue.width_microns != this.lastMediaSize_.width_microns)) {
      this.onSettingsChanged_();
    }
    this.lastMediaSize_ = newValue;
  },

  /** @private */
  onPagesPerSheetChanged_: function() {
    const pagesPerSheet =
        /** @type {number} */ (this.getSettingValue('pagesPerSheet'));

    if (pagesPerSheet == 1 ||
        this.getSettingValue('margins') ==
            print_preview.ticket_items.MarginsTypeValue.DEFAULT) {
      this.onSettingsChanged_();
    } else {
      this.setSetting(
          'margins', print_preview.ticket_items.MarginsTypeValue.DEFAULT);
    }
  },

  /**
   * Requests a preview from the native layer.
   * @return {!Promise} Promise that resolves when the preview has been
   *     generated.
   */
  getPreview_: function() {
    this.inFlightRequestId_++;
    const dpi = /** @type {{horizontal_dpi: (number | undefined),
                            vertical_dpi: (number | undefined),
                            vendor_id: (number | undefined)}} */ (
        this.getSettingValue('dpi'));
    const ticket = {
      pageRange: this.getSettingValue('ranges'),
      mediaSize: this.getSettingValue('mediaSize'),
      landscape: this.getSettingValue('layout'),
      color: this.destination.getNativeColorModel(
          /** @type {boolean} */ (this.getSettingValue('color'))),
      headerFooterEnabled: this.getSettingValue('headerFooter'),
      marginsType: this.getSettingValue('margins'),
      pagesPerSheet: this.getSettingValue('pagesPerSheet'),
      isFirstRequest: this.inFlightRequestId_ == 0,
      requestID: this.inFlightRequestId_,
      previewModifiable: this.documentInfo.isModifiable,
      fitToPageEnabled: this.getSettingValue('fitToPage'),
      scaleFactor: parseInt(this.getSettingValue('scaling'), 10),
      shouldPrintBackgrounds: this.getSettingValue('cssBackground'),
      shouldPrintSelectionOnly: this.getSettingValue('selectionOnly'),
      // NOTE: Even though the remaining fields don't directly relate to the
      // preview, they still need to be included.
      // e.g. printing::PrintSettingsFromJobSettings() still checks for them.
      collate: true,
      copies: 1,
      deviceName: this.destination.id,
      dpiHorizontal: (dpi && 'horizontal_dpi' in dpi) ? dpi.horizontal_dpi : 0,
      dpiVertical: (dpi && 'vertical_dpi' in dpi) ? dpi.vertical_dpi : 0,
      duplex: this.getSettingValue('duplex') ?
          print_preview_new.DuplexMode.LONG_EDGE :
          print_preview_new.DuplexMode.SIMPLEX,
      printToPDF: this.destination.id ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
      printWithCloudPrint: !this.destination.isLocal,
      printWithPrivet: this.destination.isPrivet,
      printWithExtension: this.destination.isExtension,
      rasterizePDF: this.getSettingValue('rasterize'),
    };

    // Set 'cloudPrintID' only if the this.destination is not local.
    if (this.destination && !this.destination.isLocal) {
      ticket.cloudPrintID = this.destination.id;
    }

    if (this.getSettingValue('margins') ==
        print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      ticket.marginsCustom = this.getSettingValue('customMargins');
    }
    return this.nativeLayer_.getPreview(JSON.stringify(ticket));
  },
});
