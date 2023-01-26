// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './icons.html.js';
import './viewer-download-controls.js';
import './viewer-page-selector.js';
import './pdf-shared.css.js';
import './shared-vars.css.js';
// <if expr="enable_ink">
import './viewer-annotations-bar.js';
import './viewer-annotations-mode-dialog.js';

// </if>

import {AnchorAlignment, CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FittingType} from '../constants.js';
import {record, UserAction} from '../metrics.js';

import {getTemplate} from './viewer-toolbar.html.js';

declare global {
  interface HTMLElementEventMap {
    'annotation-mode-toggled': CustomEvent<boolean>;
    'display-annotations-changed': CustomEvent<boolean>;
    'fit-to-changed': CustomEvent<FittingType>;
  }
}

export interface ViewerToolbarElement {
  $: {
    sidenavToggle: HTMLElement,
    menu: CrActionMenuElement,
    'two-page-view-button': HTMLButtonElement,
  };
}

export class ViewerToolbarElement extends PolymerElement {
  static get is() {
    return 'viewer-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // <if expr="enable_ink">
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
      printingEnabled: Boolean,
      rotated: Boolean,
      viewportZoom: Number,
      zoomBounds: Object,

      sidenavCollapsed: Boolean,
      twoUpViewEnabled: Boolean,

      moreMenuOpen_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      fittingType_: Number,

      fitToButtonIcon_: {
        type: String,
        computed: 'computeFitToButtonIcon_(fittingType_)',
      },

      viewportZoomPercent_: {
        type: Number,
        computed: 'computeViewportZoomPercent_(viewportZoom)',
        observer: 'viewportZoomPercentChanged_',
      },

      // <if expr="enable_ink">
      showAnnotationsModeDialog_: {
        type: Boolean,
        value: false,
      },

      showAnnotationsBar_: {
        type: Boolean,
        computed: 'computeShowAnnotationsBar_(' +
            'loading_, annotationMode, pdfAnnotationsEnabled)',
      },
      // </if>
    };
  }

  docTitle: string;
  docLength: number;
  hasEdits: boolean;
  hasEnteredAnnotationMode: boolean;
  isFormFieldFocused: boolean;
  loadProgress: number;
  pageNo: number;
  pdfAnnotationsEnabled: boolean;
  printingEnabled: boolean;
  rotated: boolean;
  viewportZoom: number;
  zoomBounds: {min: number, max: number};
  sidenavCollapsed: boolean = false;
  twoUpViewEnabled: boolean;
  private displayAnnotations_: boolean = true;
  private fittingType_: FittingType = FittingType.FIT_TO_PAGE;
  private fitToButtonIcon_: string;
  private moreMenuOpen_: boolean = false;
  private loading_: boolean = true;
  private viewportZoomPercent_: number;

  // <if expr="enable_ink">
  annotationAvailable: boolean;
  annotationMode: boolean;
  private showAnnotationsModeDialog_: boolean;
  private showAnnotationsBar_: boolean;
  // </if>

  private onSidenavToggleClick_() {
    record(UserAction.TOGGLE_SIDENAV);
    this.dispatchEvent(new CustomEvent('sidenav-toggle-click'));
  }

  private computeFitToButtonIcon_(): string {
    return this.fittingType_ === FittingType.FIT_TO_PAGE ? 'pdf:fit-to-height' :
                                                           'pdf:fit-to-width';
  }

  private computeViewportZoomPercent_(): number {
    return Math.round(100 * this.viewportZoom);
  }

  /** @return The appropriate tooltip for the current state. */
  private getFitToButtonTooltip_(
      fitToPageTooltip: string, fitToWidthTooltip: string): string {
    return this.fittingType_ === FittingType.FIT_TO_PAGE ? fitToPageTooltip :
                                                           fitToWidthTooltip;
  }

  private loadProgressChanged_() {
    this.loading_ = this.loadProgress < 100;
  }

  private viewportZoomPercentChanged_() {
    this.getZoomInput_().value = `${this.viewportZoomPercent_}%`;
  }

  // <if expr="enable_ink">
  private computeShowAnnotationsBar_(): boolean {
    return this.pdfAnnotationsEnabled && !this.loading_ && this.annotationMode;
  }
  // </if>

  private onPrintClick_() {
    this.dispatchEvent(new CustomEvent('print'));
  }

  private onRotateClick_() {
    this.dispatchEvent(new CustomEvent('rotate-left'));
  }

  private toggleDisplayAnnotations_() {
    record(UserAction.TOGGLE_DISPLAY_ANNOTATIONS);
    this.displayAnnotations_ = !this.displayAnnotations_;
    this.dispatchEvent(new CustomEvent(
        'display-annotations-changed', {detail: this.displayAnnotations_}));
    this.$.menu.close();

    // <if expr="enable_ink">
    if (!this.displayAnnotations_ && this.annotationMode) {
      this.toggleAnnotation();
    }
    // </if>
  }

  private onPresentClick_() {
    record(UserAction.PRESENT);
    this.$.menu.close();
    this.dispatchEvent(new CustomEvent('present-click'));
  }

  private onPropertiesClick_() {
    record(UserAction.PROPERTIES);
    this.$.menu.close();
    this.dispatchEvent(new CustomEvent('properties-click'));
  }

  private getAriaChecked_(checked: boolean): string {
    return checked ? 'true' : 'false';
  }

  private getAriaExpanded_(): string {
    return this.sidenavCollapsed ? 'false' : 'true';
  }

  private toggleTwoPageViewClick_() {
    const newTwoUpViewEnabled = !this.twoUpViewEnabled;
    this.dispatchEvent(
        new CustomEvent('two-up-view-changed', {detail: newTwoUpViewEnabled}));
    this.$.menu.close();
  }

  private onZoomInClick_() {
    this.dispatchEvent(new CustomEvent('zoom-in'));
  }

  private onZoomOutClick_() {
    this.dispatchEvent(new CustomEvent('zoom-out'));
  }

  forceFit(fittingType: FittingType) {
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

  private onFitToButtonClick_() {
    this.fitToggle();
  }

  private getZoomInput_(): HTMLInputElement {
    return this.shadowRoot!.querySelector('#zoom-controls input')!;
  }

  private onZoomChange_() {
    const input = this.getZoomInput_();
    let value = Number.parseInt(input.value, 10);
    value = Math.max(Math.min(value, this.zoomBounds.max), this.zoomBounds.min);
    if (this.sendZoomChanged_(value)) {
      return;
    }

    const zoomString = `${this.viewportZoomPercent_}%`;
    input.value = zoomString;
  }

  /**
   * @param value The new zoom value
   * @return Whether the zoom-changed event was sent.
   */
  private sendZoomChanged_(value: number): boolean {
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

  private onZoomInputPointerup_(e: Event) {
    (e.target as HTMLInputElement).select();
  }

  private onMoreClick_() {
    const anchor = this.shadowRoot!.querySelector<HTMLElement>('#more')!;
    this.$.menu.showAt(anchor, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  private onMoreOpenChanged_(e: CustomEvent<{value: boolean}>) {
    this.moreMenuOpen_ = e.detail.value;
  }

  private isAtMinimumZoom_(): boolean {
    return this.zoomBounds !== undefined &&
        this.viewportZoomPercent_ === this.zoomBounds.min;
  }

  private isAtMaximumZoom_(): boolean {
    return this.zoomBounds !== undefined &&
        this.viewportZoomPercent_ === this.zoomBounds.max;
  }

  // <if expr="enable_ink">
  private onDialogClose_() {
    const confirmed =
        this.shadowRoot!.querySelector(
                            'viewer-annotations-mode-dialog')!.wasConfirmed();
    this.showAnnotationsModeDialog_ = false;
    if (confirmed) {
      this.dispatchEvent(new CustomEvent('annotation-mode-dialog-confirmed'));
      this.toggleAnnotation();
    }
  }

  private onAnnotationClick_() {
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

declare global {
  interface HTMLElementTagNameMap {
    'viewer-toolbar': ViewerToolbarElement;
  }
}

customElements.define(ViewerToolbarElement.is, ViewerToolbarElement);
