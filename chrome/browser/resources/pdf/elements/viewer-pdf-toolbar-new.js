// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './icons.js';
// <if expr="chromeos">
import './viewer-annotations-bar.js';
// </if>
import './viewer-download-controls.js';
import './viewer-page-selector.js';
import './shared-css.js';
import './shared-vars.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FittingType} from '../constants.js';
// <if expr="chromeos">
import {InkController} from '../ink_controller.js';
// </if>
import {PDFMetrics, UserAction} from '../metrics.js';
// <if expr="chromeos">
import {ViewerAnnotationsModeDialogElement} from './viewer-annotations-mode-dialog.js';
// </if>

export class ViewerPdfToolbarNewElement extends PolymerElement {
  static get is() {
    return 'viewer-pdf-toolbar-new';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // <if expr="chromeos">
      annotationAvailable: Boolean,
      annotationMode: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      // </if>
      docTitle: String,
      docLength: Number,
      hasEdits: Boolean,
      hasEnteredAnnotationMode: Boolean,
      // <if expr="chromeos">
      /** @type {?InkController} */
      inkController: Object,
      // </if>
      isFormFieldFocused: Boolean,

      loadProgress: {
        type: Number,
        observer: 'loadProgressChanged_',
      },

      loading_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      pageNo: Number,
      pdfAnnotationsEnabled: Boolean,
      pdfFormSaveEnabled: Boolean,
      printingEnabled: Boolean,
      rotated: Boolean,
      viewportZoom: {
        type: Number,
        observer: 'viewportZoomChanged_',
      },
      /** @type {!{min: number, max: number}} */
      zoomBounds: Object,

      sidenavCollapsed: Boolean,
      twoUpViewEnabled: Boolean,

      moreMenuOpen_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      fittingType_: Number,

      /** @private {string} */
      fitToButtonIcon_: {
        type: String,
        computed: 'computeFitToButtonIcon_(fittingType_)',
      },

      // <if expr="chromeos">
      /** @private */
      showAnnotationsModeDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showAnnotationsBar_: {
        type: Boolean,
        computed: 'computeShowAnnotationsBar_(' +
            'loading_, annotationMode, pdfAnnotationsEnabled)',
      },
      // </if>
    };
  }

  constructor() {
    super();

    /** @type {boolean} */
    this.sidenavCollapsed = false;

    /** @private {!FittingType} */
    this.fittingType_ = FittingType.FIT_TO_PAGE;

    /** @private {boolean} */
    this.loading_ = true;

    /** @private {boolean} */
    this.displayAnnotations_ = true;

    /** @private {boolean} */
    this.moreMenuOpen_ = false;
  }

  /**
   * @return {!CrActionMenuElement}
   * @private
   */
  getMenu_() {
    return /** @type {!CrActionMenuElement} */ (
        this.shadowRoot.querySelector('cr-action-menu'));
  }

  /** @private */
  onSidenavToggleClick_() {
    PDFMetrics.record(UserAction.TOGGLE_SIDENAV);
    this.dispatchEvent(new CustomEvent('sidenav-toggle-click'));
  }

  /**
   * @return {string}
   * @private
   */
  computeFitToButtonIcon_() {
    return this.fittingType_ === FittingType.FIT_TO_PAGE ? 'pdf:fit-to-height' :
                                                           'pdf:fit-to-width';
  }

  /**
   * @param {string} fitToPageTooltip
   * @param {string} fitToWidthTooltip
   * @return {string} The appropriate tooltip for the current state
   * @private
   */
  getFitToButtonTooltip_(fitToPageTooltip, fitToWidthTooltip) {
    return this.fittingType_ === FittingType.FIT_TO_PAGE ? fitToPageTooltip :
                                                           fitToWidthTooltip;
  }

  /** @private */
  loadProgressChanged_() {
    this.loading_ = this.loadProgress < 100;
  }

  /** @private */
  viewportZoomChanged_() {
    const zoom = Math.round(this.viewportZoom * 100);
    this.getZoomInput_().value = `${zoom}%`;
  }

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeShowAnnotationsBar_() {
    return this.pdfAnnotationsEnabled && !this.loading_ && this.annotationMode;
  }
  // </if>

  /** @private */
  onPrintClick_() {
    this.dispatchEvent(new CustomEvent('print'));
  }

  /** @private */
  onRotateClick_() {
    this.dispatchEvent(new CustomEvent('rotate-left'));
  }

  /** @private */
  toggleDisplayAnnotations_() {
    PDFMetrics.record(UserAction.TOGGLE_DISPLAY_ANNOTATIONS);
    this.displayAnnotations_ = !this.displayAnnotations_;
    this.dispatchEvent(new CustomEvent(
        'display-annotations-changed', {detail: this.displayAnnotations_}));
    this.getMenu_().close();

    // <if expr="chromeos">
    if (!this.displayAnnotations_ && this.annotationMode) {
      this.toggleAnnotation();
    }
    // </if>
  }

  /**
   * @param {boolean} checked
   * @return {string}
   */
  getSinglePageAriaChecked_(checked) {
    return checked ? 'false' : 'true';
  }

  /**
   * @param {boolean} checked
   * @return {string}
   */
  getTwoPageViewAriaChecked_(checked) {
    return checked ? 'true' : 'false';
  }

  /**
   * @param {boolean} checked
   * @return {string}
   */
  getShowAnnotationsAriaChecked_(checked) {
    return checked ? 'true' : 'false';
  }

  /** @return {string} */
  getAriaExpanded_() {
    return this.sidenavCollapsed ? 'false' : 'true';
  }

  /** @private */
  toggleTwoPageViewClick_() {
    const newTwoUpViewEnabled = !this.twoUpViewEnabled;
    this.dispatchEvent(
        new CustomEvent('two-up-view-changed', {detail: newTwoUpViewEnabled}));
    this.getMenu_().close();
  }

  /** @private */
  onZoomInClick_() {
    this.dispatchEvent(new CustomEvent('zoom-in'));
  }

  /** @private */
  onZoomOutClick_() {
    this.dispatchEvent(new CustomEvent('zoom-out'));
  }

  /** @param {!FittingType} fittingType */
  forceFit(fittingType) {
    // The fitting type is the new state. We want to set the button fitting type
    // to the opposite value.
    this.fittingType_ = fittingType === FittingType.FIT_TO_WIDTH ?
        FittingType.FIT_TO_PAGE :
        FittingType.FIT_TO_WIDTH;
  }

  fitToggle() {
    const newState = this.fittingType_ === FittingType.FIT_TO_PAGE ?
        FittingType.FIT_TO_WIDTH :
        FittingType.FIT_TO_PAGE;
    this.dispatchEvent(
        new CustomEvent('fit-to-changed', {detail: this.fittingType_}));
    this.fittingType_ = newState;
  }

  /** @private */
  onFitToButtonClick_() {
    this.fitToggle();
  }

  /**
   * @return {!HTMLInputElement}
   * @private
   */
  getZoomInput_() {
    return /** @type {!HTMLInputElement} */ (
        this.shadowRoot.querySelector('#zoom-controls input'));
  }

  /** @private */
  onZoomChange_() {
    const input = this.getZoomInput_();
    let value = Number.parseInt(input.value, 10);
    value = Math.max(Math.min(value, this.zoomBounds.max), this.zoomBounds.min);
    if (this.sendZoomChanged_(value)) {
      return;
    }

    const zoom = Math.round(this.viewportZoom * 100);
    const zoomString = `${zoom}%`;
    input.value = zoomString;
  }

  /**
   * @param {number} value The new zoom value
   * @return {boolean} Whether the zoom-changed event was sent.
   * @private
   */
  sendZoomChanged_(value) {
    if (Number.isNaN(value)) {
      return false;
    }

    // The viewport can have non-integer zoom values.
    if (Math.abs(this.viewportZoom * 100 - value) < 0.5) {
      return false;
    }

    this.dispatchEvent(new CustomEvent('zoom-changed', {detail: value}));
    return true;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onZoomInputPointerup_(e) {
    /* @type {!HTMLInputElement} */ (e.target).select();
  }

  /** @private */
  onMoreClick_() {
    const anchor =
        /** @type {!HTMLElement} */ (this.shadowRoot.querySelector('#more'));
    this.getMenu_().showAt(anchor, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  /**
   * @param {!CustomEvent<!{value: boolean}>} e
   * @private
   */
  onMoreOpenChanged_(e) {
    this.moreMenuOpen_ = e.detail.value;
  }

  // <if expr="chromeos">
  /** @private */
  onDialogClose_() {
    const confirmed =
        /** @type {!ViewerAnnotationsModeDialogElement} */ (
            this.shadowRoot.querySelector('viewer-annotations-mode-dialog'))
            .wasConfirmed();
    this.showAnnotationsModeDialog_ = false;
    if (confirmed) {
      this.dispatchEvent(new CustomEvent('annotation-mode-dialog-confirmed'));
      this.toggleAnnotation();
    }
  }

  /** @private */
  onAnnotationClick_() {
    if (!this.rotated && !this.twoUpViewEnabled) {
      this.toggleAnnotation();
      return;
    }

    this.showAnnotationsModeDialog_ = true;
  }

  toggleAnnotation() {
    const newAnnotationMode = !this.annotationMode;
    this.dispatchEvent(new CustomEvent(
        'annotation-mode-toggled', {detail: newAnnotationMode}));

    if (newAnnotationMode && !this.displayAnnotations_) {
      this.toggleDisplayAnnotations_();
    }
  }
  // </if>
}

customElements.define(
    ViewerPdfToolbarNewElement.is, ViewerPdfToolbarNewElement);
