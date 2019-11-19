// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './print_preview_vars_css.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DarkModeBehavior} from '../dark_mode_behavior.js';
import {Coordinate2d} from '../data/coordinate2d.js';
import {Destination} from '../data/destination.js';
import {getPrinterTypeForDestination} from '../data/destination_match.js';
import {CustomMarginsOrientation, Margins, MarginsSetting, MarginsType} from '../data/margins.js';
import {MeasurementSystem} from '../data/measurement_system.js';
import {DuplexMode} from '../data/model.js';
import {PrintableArea} from '../data/printable_area.js';
import {ScalingType} from '../data/scaling.js';
import {Size} from '../data/size.js';
import {Error, State} from '../data/state.js';
import {NativeLayer} from '../native_layer.js';
import {areRangesEqual} from '../print_preview_utils.js';

import {MARGIN_KEY_MAP} from './margin_control_container.js';
import {PluginProxy} from './plugin_proxy.js';
import {SettingsBehavior} from './settings_behavior.js';

/**
 * @typedef {{
 *   width_microns: number,
 *   height_microns: number,
 * }}
 */
let MediaSizeValue;

/** @enum {string} */
export const PreviewAreaState = {
  NO_PLUGIN: 'no-plugin',
  LOADING: 'loading',
  DISPLAY_PREVIEW: 'display-preview',
  OPEN_IN_PREVIEW_LOADING: 'open-in-preview-loading',
  OPEN_IN_PREVIEW_LOADED: 'open-in-preview-loaded',
  ERROR: 'error',
};

Polymer({
  is: 'print-preview-preview-area',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
    SettingsBehavior,
    I18nBehavior,
    DarkModeBehavior,
  ],

  properties: {
    /** @type {Destination} */
    destination: Object,

    documentModifiable: Boolean,

    /** @type {!Error} */
    error: {
      type: Number,
      notify: true,
    },

    /** @type {Margins} */
    margins: Object,

    /** @type {?MeasurementSystem} */
    measurementSystem: Object,

    /** @type {!Size} */
    pageSize: Object,

    /** @type {!PreviewAreaState} */
    previewState: {
      type: String,
      notify: true,
      value: PreviewAreaState.LOADING,
    },

    /** @type {!State} */
    state: Number,

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
    'onDarkModeChanged_(inDarkMode)',
    'pluginOrDocumentStatusChanged_(pluginLoaded_, documentReady_)',
    'onStateOrErrorChange_(state, error)',
  ],

  /** @private {?NativeLayer} */
  nativeLayer_: null,

  /** @private {?Object} */
  lastTicket_: null,

  /** @private {number} */
  inFlightRequestId_: -1,

  /** @private {?PluginProxy} */
  pluginProxy_: null,

  /** @private {?function(!KeyboardEvent)} */
  keyEventCallback_: null,

  /** @override */
  attached: function() {
    this.nativeLayer_ = NativeLayer.getInstance();
    this.addWebUIListener(
        'page-preview-ready', this.onPagePreviewReady_.bind(this));

    if (!this.pluginProxy_.checkPluginCompatibility(assert(
            this.$$('.preview-area-compatibility-object-out-of-process')))) {
      this.error = Error.NO_PLUGIN;
      this.previewState = PreviewAreaState.ERROR;
    }
  },

  /** @override */
  created: function() {
    this.pluginProxy_ = PluginProxy.getInstance();
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
      if (fromElement == marginControlContainer) {
        return;
      }

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
      if (toElement == marginControlContainer) {
        return;
      }

      toElement = toElement.parentElement;
    }
    marginControlContainer.setInvisible(true);
  },

  /** @private */
  pluginOrDocumentStatusChanged_: function() {
    if (!this.pluginLoaded_ || !this.documentReady_ ||
        this.previewState === PreviewAreaState.ERROR) {
      return;
    }

    this.previewState =
        this.previewState == PreviewAreaState.OPEN_IN_PREVIEW_LOADING ?
        PreviewAreaState.OPEN_IN_PREVIEW_LOADED :
        PreviewAreaState.DISPLAY_PREVIEW;
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
    return this.previewState == PreviewAreaState.DISPLAY_PREVIEW;
  },

  /**
   * @return {boolean} Whether the preview is currently loading.
   * @private
   */
  isPreviewLoading_: function() {
    return this.previewState == PreviewAreaState.LOADING;
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
    return this.error === Error.UNSUPPORTED_PRINTER;
  },

  /**
   * @return {string} The current preview area message to display.
   * @private
   */
  currentMessage_: function() {
    switch (this.previewState) {
      case PreviewAreaState.LOADING:
        return this.i18n('loading');
      case PreviewAreaState.DISPLAY_PREVIEW:
        return '';
      // <if expr="is_macosx">
      case PreviewAreaState.OPEN_IN_PREVIEW_LOADING:
      case PreviewAreaState.OPEN_IN_PREVIEW_LOADED:
        return this.i18n('openingPDFInPreview');
      // </if>
      case PreviewAreaState.ERROR:
        // The preview area is responsible for displaying all errors except
        // print failed and cloud print error.
        return this.getErrorMessage_();
      default:
        return '';
    }
  },

  /**
   * @param {boolean} forceUpdate Whether to force the preview area to update
   *     regardless of whether the print ticket has changed.
   */
  startPreview: function(forceUpdate) {
    if (!this.hasTicketChanged_() && !forceUpdate &&
        this.previewState !== PreviewAreaState.ERROR) {
      return;
    }
    this.previewState = PreviewAreaState.LOADING;
    this.documentReady_ = false;
    this.getPreview_().then(
        previewUid => {
          if (!this.documentModifiable) {
            this.onPreviewStart_(previewUid, -1);
          }
          this.documentReady_ = true;
        },
        type => {
          if (/** @type{string} */ (type) == 'SETTINGS_INVALID') {
            this.error = Error.INVALID_PRINTER;
            this.previewState = PreviewAreaState.ERROR;
          } else if (/** @type{string} */ (type) != 'CANCELLED') {
            this.error = Error.PREVIEW_FAILED;
            this.previewState = PreviewAreaState.ERROR;
          }
        });
  },

  // <if expr="is_macosx">
  /** Set the preview state to display the "opening in preview" message. */
  setOpeningPdfInPreview: function() {
    assert(isMac);
    this.previewState = this.previewState == PreviewAreaState.LOADING ?
        PreviewAreaState.OPEN_IN_PREVIEW_LOADING :
        PreviewAreaState.OPEN_IN_PREVIEW_LOADED;
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
      this.pluginProxy_.setKeyEventCallback(this.keyEventCallback_);
      this.$$('.preview-area-plugin-wrapper')
          .appendChild(
              /** @type {Node} */ (plugin));
      this.pluginProxy_.setLoadCallback(this.onPluginLoad_.bind(this));
      this.pluginProxy_.setViewportChangedCallback(
          this.onPreviewVisualStateChange_.bind(this));
    }

    this.pluginLoaded_ = false;
    if (this.inDarkMode) {
      this.pluginProxy_.darkModeChanged(true);
    }
    this.pluginProxy_.resetPrintPreviewMode(
        previewUid, index, !this.getSettingValue('color'),
        /** @type {!Array<number>} */ (this.getSettingValue('pages')),
        this.documentModifiable);
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
      this.error = Error.PREVIEW_FAILED;
      this.previewState = PreviewAreaState.ERROR;
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
    // Ensure the PDF viewer isn't tabbable if the window is small enough that
    // the zoom toolbar isn't displayed.
    const tabindex = viewportWidth < 300 || viewportHeight < 200 ? '-1' : '0';
    this.$$('.preview-area-plugin').setAttribute('tabindex', tabindex);
    this.$.marginControlContainer.updateTranslationTransform(
        new Coordinate2d(pageX, pageY));
    this.$.marginControlContainer.updateScaleTransform(
        pageWidth / this.pageSize.width);
    this.$.marginControlContainer.updateClippingMask(
        new Size(viewportWidth, viewportHeight));
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
    if (this.inFlightRequestId_ != previewResponseId) {
      return;
    }
    const pageNumber = pageIndex + 1;
    let index = this.getSettingValue('pages').indexOf(pageNumber);
    // When pagesPerSheet > 1, the backend will always return page indices 0 to
    // N-1, where N is the total page count of the N-upped document.
    const pagesPerSheet =
        /** @type {number} */ (this.getSettingValue('pagesPerSheet'));
    if (pagesPerSheet > 1) {
      index = pageIndex;
    }
    if (index == 0) {
      this.onPreviewStart_(previewUid, pageIndex);
    }
    if (index != -1) {
      this.pluginProxy_.loadPreviewPage(previewUid, pageIndex, index);
    }
  },

  /** @private */
  onDarkModeChanged_: function() {
    if (this.pluginProxy_.pluginReady()) {
      this.pluginProxy_.darkModeChanged(this.inDarkMode);
    }

    if (this.previewState === PreviewAreaState.DISPLAY_PREVIEW) {
      this.startPreview(true);
    }
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
    if (['INPUT', 'SELECT', 'EMBED'].includes(tagName)) {
      return;
    }

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
   * Sends a message to the plugin to hide the toolbars after a delay.
   */
  hideToolbars: function() {
    if (!this.pluginProxy_.pluginReady()) {
      return;
    }

    this.pluginProxy_.hideToolbars();
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
    if (!this.pluginProxy_.pluginReady()) {
      return;
    }

    // When hovering over the plugin (which may be in a separate iframe)
    // pointer events will be sent to the frame. When dragging the margins,
    // we don't want this to happen as it can cause the margin to stop
    // being draggable.
    this.pluginProxy_.setPointerEvents(!e.detail);
  },

  /**
   * @param {!CustomEvent<{x: number, y: number}>} e Contains information about
   *     where the plugin should scroll to.
   * @private
   */
  onTextFocusPosition_: function(e) {
    // TODO(tkent): This is a workaround of a preview-area scrolling
    // issue. Blink scrolls preview-area on focus, but we don't want it.  We
    // should adjust scroll position of PDF preview and positions of
    // MarginContgrols here, or restructure the HTML so that the PDF review
    // and MarginControls are on the single scrollable container.
    // crbug.com/601341
    this.scrollTop = 0;
    this.scrollLeft = 0;

    const position = e.detail;
    if (position.x === 0 && position.y === 0) {
      return;
    }

    this.pluginProxy_.scrollPosition(position.x, position.y);
  },

  /**
   * @return {boolean} Whether margin settings are valid for the print ticket.
   * @private
   */
  marginsValid_: function() {
    const type = this.getSettingValue('margins');
    if (!Object.values(MarginsType).includes(type)) {
      // Unrecognized margins type.
      return false;
    }

    if (type !== MarginsType.CUSTOM) {
      return true;
    }

    const customMargins = this.getSettingValue('customMargins');
    return customMargins.marginTop !== undefined &&
        customMargins.marginLeft !== undefined &&
        customMargins.marginBottom !== undefined &&
        customMargins.marginRight !== undefined;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasTicketChanged_: function() {
    if (!this.marginsValid_()) {
      // Log so that we can try to debug how this occurs. See
      // https://crbug.com/942211
      console.warn('Requested preview with invalid margins');
      return false;
    }

    if (!this.lastTicket_) {
      return true;
    }

    const lastTicket = this.lastTicket_;

    // Margins
    const newMarginsType = this.getSettingValue('margins');
    if (newMarginsType !== lastTicket.marginsType &&
        newMarginsType !== MarginsType.CUSTOM) {
      return true;
    }

    if (newMarginsType === MarginsType.CUSTOM) {
      const customMargins =
          /** @type {!MarginsSetting} */ (
              this.getSettingValue('customMargins'));

      // Change in custom margins values.
      if (!!lastTicket.marginsCustom &&
          (lastTicket.marginsCustom.marginTop != customMargins.marginTop ||
           lastTicket.marginsCustom.marginLeft != customMargins.marginLeft ||
           lastTicket.marginsCustom.marginRight != customMargins.marginRight ||
           lastTicket.marginsCustom.marginBottom !=
               customMargins.marginBottom)) {
        return true;
      }

      // Changed to custom margins from a different margins type.
      if (!this.margins) {
        // Log so that we can try to debug how this occurs. See
        // https://crbug.com/942211
        console.warn('Requested preview with undefined document margins');
        return false;
      }

      const customMarginsChanged =
          Object.values(CustomMarginsOrientation).some(side => {
            return this.margins.get(side) !==
                customMargins[MARGIN_KEY_MAP.get(side)];
          });
      if (customMarginsChanged) {
        return true;
      }
    }

    // Simple settings: ranges, layout, header/footer, pages per sheet, fit to
    // page, css background, selection only, rasterize, scaling, dpi
    if (!areRangesEqual(
            /** @type {!Array<{from: number, to: number}>} */
            (this.getSettingValue('ranges')), lastTicket.pageRange) ||
        this.getSettingValue('layout') !== lastTicket.landscape ||
        this.getColorForTicket_() !== lastTicket.color ||
        this.getSettingValue('headerFooter') !==
            lastTicket.headerFooterEnabled ||
        this.getSettingValue('cssBackground') !==
            lastTicket.shouldPrintBackgrounds ||
        this.getSettingValue('selectionOnly') !==
            lastTicket.shouldPrintSelectionOnly ||
        this.getSettingValue('rasterize') !== lastTicket.rasterizePDF ||
        this.isScalingChanged_(lastTicket)) {
      return true;
    }

    // Pages per sheet. If margins are non-default, wait for the return to
    // default margins to trigger a request.
    if (this.getSettingValue('pagesPerSheet') !== lastTicket.pagesPerSheet &&
        this.getSettingValue('margins') === MarginsType.DEFAULT) {
      return true;
    }

    // Media size
    const newValue =
        /** @type {!MediaSizeValue} */ (this.getSettingValue('mediaSize'));
    if (newValue.height_microns != lastTicket.mediaSize.height_microns ||
        newValue.width_microns != lastTicket.mediaSize.width_microns ||
        (this.destination.id !== lastTicket.deviceName &&
         this.getSettingValue('margins') === MarginsType.MINIMUM)) {
      return true;
    }

    // Destination
    if (getPrinterTypeForDestination(this.destination) !==
        lastTicket.printerType) {
      return true;
    }

    return false;
  },

  /** @return {number} Native color model of the destination. */
  getColorForTicket_: function() {
    return this.destination.getNativeColorModel(
        /** @type {boolean} */ (this.getSettingValue('color')));
  },

  /** @return {number} Scale factor for print ticket. */
  getScaleFactorForTicket_: function() {
    return this.getSettingValue(this.getScalingSettingKey_()) ===
            ScalingType.CUSTOM ?
        parseInt(this.getSettingValue('scaling'), 10) :
        100;
  },

  /** @return {string} Appropriate key for the scaling type setting. */
  getScalingSettingKey_: function() {
    return this.getSetting('scalingTypePdf').available ? 'scalingTypePdf' :
                                                         'scalingType';
  },

  /**
   * @param {Object} lastTicket Last print ticket.
   * @return {boolean} Whether new scaling settings update the previewed
   *     document.
   */
  isScalingChanged_: function(lastTicket) {
    // Preview always updates if the scale factor is changed.
    if (this.getScaleFactorForTicket_() !== lastTicket.scaleFactor) {
      return true;
    }

    // If both scale factors and type match, no scaling change happened.
    const scalingType = this.getSettingValue(this.getScalingSettingKey_());
    if (scalingType === lastTicket.scalingType) {
      return false;
    }

    // Scaling doesn't always change because of a scalingType change. Changing
    // between custom scaling with a scale factor of 100 and default scaling
    // makes no difference.
    const defaultToCustom = scalingType === ScalingType.DEFAULT &&
        lastTicket.scalingType === ScalingType.CUSTOM;
    const customToDefault = scalingType === ScalingType.CUSTOM &&
        lastTicket.scalingType === ScalingType.DEFAULT;

    return !defaultToCustom && !customToDefault;
  },

  /**
   * @param {string} dpiField The field in dpi to retrieve.
   * @return {number} Field value.
   */
  getDpiForTicket_: function(dpiField) {
    const dpi =
        /**
           @type {{horizontal_dpi: (number | undefined),
                    vertical_dpi: (number | undefined),
                    vendor_id: (number | undefined)}}
         */
        (this.getSettingValue('dpi'));
    const value = (dpi && dpiField in dpi) ? dpi[dpiField] : 0;
    return value;
  },

  /**
   * Requests a preview from the native layer.
   * @return {!Promise} Promise that resolves when the preview has been
   *     generated.
   */
  getPreview_: function() {
    this.inFlightRequestId_++;
    const ticket = {
      pageRange: this.getSettingValue('ranges'),
      mediaSize: this.getSettingValue('mediaSize'),
      landscape: this.getSettingValue('layout'),
      color: this.getColorForTicket_(),
      headerFooterEnabled: this.getSettingValue('headerFooter'),
      marginsType: this.getSettingValue('margins'),
      pagesPerSheet: this.getSettingValue('pagesPerSheet'),
      isFirstRequest: this.inFlightRequestId_ == 0,
      requestID: this.inFlightRequestId_,
      previewModifiable: this.documentModifiable,
      scaleFactor: this.getScaleFactorForTicket_(),
      scalingType: this.getSettingValue(this.getScalingSettingKey_()),
      shouldPrintBackgrounds: this.getSettingValue('cssBackground'),
      shouldPrintSelectionOnly: this.getSettingValue('selectionOnly'),
      // NOTE: Even though the remaining fields don't directly relate to the
      // preview, they still need to be included.
      // e.g. printing::PrintSettingsFromJobSettings() still checks for them.
      collate: true,
      copies: 1,
      deviceName: this.destination.id,
      dpiHorizontal: this.getDpiForTicket_('horizontal_dpi'),
      dpiVertical: this.getDpiForTicket_('vertical_dpi'),
      duplex: this.getSettingValue('duplex') ? DuplexMode.LONG_EDGE :
                                               DuplexMode.SIMPLEX,
      printerType: getPrinterTypeForDestination(this.destination),
      rasterizePDF: this.getSettingValue('rasterize'),
    };

    // Set 'cloudPrintID' only if the this.destination is not local.
    if (this.destination && !this.destination.isLocal) {
      ticket.cloudPrintID = this.destination.id;
    }

    if (this.getSettingValue('margins') == MarginsType.CUSTOM) {
      ticket.marginsCustom = this.getSettingValue('customMargins');
    }
    this.lastTicket_ = ticket;

    this.fire('preview-start', this.inFlightRequestId_);
    return this.nativeLayer_.getPreview(JSON.stringify(ticket));
  },

  /** @private */
  onStateOrErrorChange_: function() {
    if ((this.state === State.ERROR || this.state === State.FATAL_ERROR) &&
        this.getErrorMessage_() !== '') {
      this.previewState = PreviewAreaState.ERROR;
    }
  },

  /** @return {string} The error message to display in the preview area. */
  getErrorMessage_: function() {
    switch (this.error) {
      case Error.INVALID_PRINTER:
        return this.i18nAdvanced('invalidPrinterSettings', {
          substitutions: [],
          tags: ['BR'],
        });
      case Error.UNSUPPORTED_PRINTER:
        return this.i18nAdvanced('unsupportedCloudPrinter', {
          substitutions: [],
          tags: ['BR'],
        });
      // <if expr="chromeos">
      case Error.NO_DESTINATIONS:
        return this.i18n('noDestinationsMessage');
      // </if>
      case Error.NO_PLUGIN:
        return this.i18n('noPlugin');
      case Error.PREVIEW_FAILED:
        return this.i18n('previewFailed');
      default:
        return '';
    }
  },
});
