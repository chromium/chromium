// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';
import './icons.html.js';
import './viewer_download_controls.js';
import './viewer_page_selector.js';
import './shared_vars.css.js';
// <if expr="enable_ink">
import './viewer_annotations_bar.js';
import './viewer_annotations_mode_dialog.js';
// </if>

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
// <if expr="enable_pdf_ink2">
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
// </if>
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {FittingType, FormFieldFocusType} from '../constants.js';
// <if expr="enable_pdf_ink2">
import {PluginController, PluginControllerEventType} from '../controller.js';
// </if>
import {record, UserAction} from '../metrics.js';

import {getCss} from './viewer_toolbar.css.js';
import {getHtml} from './viewer_toolbar.html.js';

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
    'present-button': HTMLButtonElement,
    'two-page-view-button': HTMLButtonElement,
  };
}

export class ViewerToolbarElement extends CrLitElement {
  static get is() {
    return 'viewer-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // <if expr="enable_ink or enable_pdf_ink2">
      annotationAvailable: {type: Boolean},
      annotationMode: {
        type: Boolean,
        reflect: true,
      },
      // </if>

      // <if expr="enable_pdf_ink2">
      canRedoAnnotation_: {type: Boolean},
      canUndoAnnotation_: {type: Boolean},
      // </if>

      docTitle: {type: String},
      docLength: {type: Number},
      embeddedViewer: {type: Boolean},
      hasEdits: {type: Boolean},
      hasEnteredAnnotationMode: {type: Boolean},
      // <if expr="enable_pdf_ink2">
      hasInk2Edits: {type: Boolean},
      // </if>
      formFieldFocus: {type: String},
      loadProgress: {type: Number},

      loading_: {
        type: Boolean,
        reflect: true,
      },

      pageNo: {type: Number},
      pdfAnnotationsEnabled: {type: Boolean},
      pdfCr23Enabled: {type: Boolean},
      // <if expr="enable_pdf_ink2">
      pdfInk2Enabled: {type: Boolean},
      // </if>

      printingEnabled: {type: Boolean},
      rotated: {type: Boolean},
      viewportZoom: {type: Number},
      zoomBounds: {type: Object},
      sidenavCollapsed: {type: Boolean},
      twoUpViewEnabled: {type: Boolean},

      moreMenuOpen_: {
        type: Boolean,
        reflect: true,
      },

      fittingType_: {type: Number},
      viewportZoomPercent_: {type: Number},

      // <if expr="enable_ink">
      showAnnotationsModeDialog_: {type: Boolean},
      // </if> enable_ink
    };
  }

  docTitle: string = '';
  docLength: number = 0;
  embeddedViewer: boolean = false;
  hasEdits: boolean = false;
  hasEnteredAnnotationMode: boolean = false;
  // <if expr="enable_pdf_ink2">
  hasInk2Edits: boolean = false;
  // </if>
  formFieldFocus: FormFieldFocusType = FormFieldFocusType.NONE;
  loadProgress: number = 0;
  pageNo: number = 0;
  pdfAnnotationsEnabled: boolean = false;
  pdfCr23Enabled: boolean = false;
  printingEnabled: boolean = false;
  rotated: boolean = false;
  viewportZoom: number = 0;
  zoomBounds: {min: number, max: number} = {min: 0, max: 0};
  sidenavCollapsed: boolean = false;
  twoUpViewEnabled: boolean = false;
  protected displayAnnotations_: boolean = true;
  private fittingType_: FittingType = FittingType.FIT_TO_PAGE;
  protected moreMenuOpen_: boolean = false;
  protected loading_: boolean = true;
  private viewportZoomPercent_: number = 0;

  // <if expr="enable_ink or enable_pdf_ink2">
  annotationAvailable: boolean = false;
  annotationMode: boolean = false;
  // </if>

  // <if expr="enable_ink">
  protected showAnnotationsModeDialog_: boolean = false;
  // </if>

  // <if expr="enable_pdf_ink2">
  pdfInk2Enabled: boolean = false;
  protected canRedoAnnotation_: boolean = false;
  protected canUndoAnnotation_: boolean = false;
  private currentStroke: number = 0;
  private mostRecentStroke: number = 0;
  private pluginController_: PluginController = PluginController.getInstance();
  private tracker_: EventTracker = new EventTracker();

  constructor() {
    super();

    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.FINISH_INK_STROKE,
        this.handleFinishInkStroke_.bind(this));
  }
  // </if>

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('loadProgress')) {
      this.loading_ = this.loadProgress < 100;
    }

    if (changedProperties.has('viewportZoom')) {
      this.viewportZoomPercent_ = Math.round(100 * this.viewportZoom);
    }

    // <if expr="enable_pdf_ink2">
    if (changedProperties.has('formFieldFocus')) {
      this.updateCanUndoRedo_();
    }
    // </if>
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // viewportZoomPercent_ always updates with viewportZoom, see above.
    if (changedProperties.has('viewportZoom')) {
      this.getZoomInput_().value = `${this.viewportZoomPercent_}%`;
    }
  }

  protected onSidenavToggleClick_() {
    record(UserAction.TOGGLE_SIDENAV);
    this.dispatchEvent(new CustomEvent('sidenav-toggle-click'));
  }

  protected iconsetName_(): string {
    return this.pdfCr23Enabled ? 'pdf-cr23' : 'pdf';
  }

  protected fitToButtonIcon_(): string {
    return this.iconsetName_() +
        (this.fittingType_ === FittingType.FIT_TO_PAGE ? ':fit-to-height' :
                                                         ':fit-to-width');
  }

  // TODO(crbug.com/360265881): Remove conditional icons after the UI refresh
  // fully launches.
  protected menuIcon_(): string {
    return this.pdfCr23Enabled ? 'pdf-cr23:menu' : 'cr20:menu';
  }

  protected moreIcon_(): string {
    return this.pdfCr23Enabled ? 'pdf-cr23:more' : 'cr:more-vert';
  }

  protected printIcon_(): string {
    return this.pdfCr23Enabled ? 'pdf-cr23:print' : 'cr:print';
  }

  /** @return The appropriate tooltip for the current state. */
  protected getFitToButtonTooltip_(
      fitToPageTooltip: string, fitToWidthTooltip: string): string {
    return this.fittingType_ === FittingType.FIT_TO_PAGE ? fitToPageTooltip :
                                                           fitToWidthTooltip;
  }

  // <if expr="enable_ink">
  protected showInkAnnotationButton_(): boolean {
    // <if expr="enable_pdf_ink2">
    if (this.pdfInk2Enabled) {
      return false;
    }
    // </if> enable_pdf_ink2

    return this.pdfAnnotationsEnabled;
  }
  // </if> enable_ink

  // <if expr="enable_pdf_ink2">
  protected showInk2Buttons_(): boolean {
    return this.pdfInk2Enabled && this.pdfAnnotationsEnabled;
  }
  // </if>

  // <if expr="enable_ink or enable_pdf_ink2">
  protected showAnnotationsBar_(): boolean {
    return this.pdfAnnotationsEnabled && !this.loading_ && this.annotationMode;
  }
  // </if>

  protected onPrintClick_() {
    this.dispatchEvent(new CustomEvent('print'));
  }

  protected onRotateClick_() {
    this.dispatchEvent(new CustomEvent('rotate-left'));
  }

  protected toggleDisplayAnnotations_() {
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

  protected onPresentClick_() {
    record(UserAction.PRESENT);
    this.$.menu.close();
    this.dispatchEvent(new CustomEvent('present-click'));
  }

  protected onPropertiesClick_() {
    record(UserAction.PROPERTIES);
    this.$.menu.close();
    this.dispatchEvent(new CustomEvent('properties-click'));
  }

  protected getAriaChecked_(checked: boolean): string {
    return checked ? 'true' : 'false';
  }

  protected getAriaExpanded_(): string {
    return this.sidenavCollapsed ? 'false' : 'true';
  }

  protected toggleTwoPageViewClick_() {
    const newTwoUpViewEnabled = !this.twoUpViewEnabled;
    this.dispatchEvent(
        new CustomEvent('two-up-view-changed', {detail: newTwoUpViewEnabled}));
    this.$.menu.close();
  }

  protected onZoomInClick_() {
    this.dispatchEvent(new CustomEvent('zoom-in'));
  }

  protected onZoomOutClick_() {
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

  protected onFitToButtonClick_() {
    this.fitToggle();
  }

  private getZoomInput_(): HTMLInputElement {
    return this.shadowRoot!.querySelector('#zoom-controls input')!;
  }

  protected onZoomChange_() {
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

  protected onZoomInputPointerup_(e: Event) {
    (e.target as HTMLInputElement).select();
  }

  protected onMoreClick_() {
    const anchor = this.shadowRoot!.querySelector<HTMLElement>('#more')!;
    this.$.menu.showAt(anchor, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  protected onMoreOpenChanged_(e: CustomEvent<{value: boolean}>) {
    this.moreMenuOpen_ = e.detail.value;
  }

  protected isAtMinimumZoom_(): boolean {
    return this.zoomBounds !== undefined &&
        this.viewportZoomPercent_ === this.zoomBounds.min;
  }

  protected isAtMaximumZoom_(): boolean {
    return this.zoomBounds !== undefined &&
        this.viewportZoomPercent_ === this.zoomBounds.max;
  }

  // <if expr="enable_ink">
  protected onDialogClose_() {
    const confirmed =
        this.shadowRoot!.querySelector(
                            'viewer-annotations-mode-dialog')!.wasConfirmed();
    this.showAnnotationsModeDialog_ = false;
    if (confirmed) {
      this.dispatchEvent(new CustomEvent('annotation-mode-dialog-confirmed'));
      this.toggleAnnotation();
    }
  }
  // </if>

  // <if expr="enable_ink or enable_pdf_ink2">
  protected onAnnotationClick_() {
    // <if expr="enable_pdf_ink2">
    if (this.pdfInk2Enabled) {
      this.toggleAnnotation();
      return;
    }
    // </if> enable_pdf_ink2

    // <if expr="enable_ink">
    if (!this.rotated && !this.twoUpViewEnabled) {
      this.toggleAnnotation();
      return;
    }

    this.showAnnotationsModeDialog_ = true;
    // </if> enable_ink
  }

  toggleAnnotation() {
    const newAnnotationMode = !this.annotationMode;
    this.dispatchEvent(new CustomEvent(
        'annotation-mode-toggled', {detail: newAnnotationMode}));

    // <if expr="enable_pdf_ink2">
    // Don't toggle display annotations for Ink2.
    if (this.pdfInk2Enabled) {
      return;
    }
    // </if> enable_pdf_ink2

    if (newAnnotationMode && !this.displayAnnotations_) {
      this.toggleDisplayAnnotations_();
    }
  }
  // </if> enable_ink or enable_pdf_ink2

  // <if expr="enable_pdf_ink2">
  /**
   * Handles whether the undo and redo buttons should be enabled or disabled
   * when a new ink stroke is added to the page.
   */
  private handleFinishInkStroke_() {
    this.currentStroke++;
    this.mostRecentStroke = this.currentStroke;

    // When a new stroke is added, it can always be undone. Since it's the most
    // recent stroke, the redo action cannot be performed.
    this.canUndoAnnotation_ = true;
    this.canRedoAnnotation_ = false;
  }

  /**
   * Undo an annotation stroke, if possible.
   */
  undo() {
    if (!this.canUndoAnnotation_) {
      return;
    }

    assert(this.currentStroke > 0);
    assert(this.formFieldFocus !== FormFieldFocusType.TEXT);

    this.pluginController_.undo();
    this.currentStroke--;

    this.updateCanUndoRedo_();
    this.dispatchEvent(new CustomEvent(
        'strokes-updated',
        {detail: this.currentStroke, bubbles: true, composed: true}));
  }

  /**
   * Redo an annotation stroke, if possible.
   */
  redo() {
    if (!this.canRedoAnnotation_) {
      return;
    }

    assert(this.currentStroke < this.mostRecentStroke);
    assert(this.formFieldFocus !== FormFieldFocusType.TEXT);

    this.pluginController_.redo();
    this.currentStroke++;
    this.updateCanUndoRedo_();
    this.dispatchEvent(new CustomEvent(
        'strokes-updated',
        {detail: this.currentStroke, bubbles: true, composed: true}));
  }

  /**
   * Update whether the undo and redo buttons should be enabled or disabled.
   * Both buttons should be disabled when a text form field has focus. Undo and
   * redo should be disabled when there are no possible undo and redo actions
   * respectively.
   */
  private updateCanUndoRedo_() {
    const isTextFormFieldFocused =
        this.formFieldFocus === FormFieldFocusType.TEXT;
    this.canUndoAnnotation_ = !isTextFormFieldFocused && this.currentStroke > 0;
    this.canRedoAnnotation_ =
        !isTextFormFieldFocused && this.currentStroke < this.mostRecentStroke;
  }

  /**
   * Reset the stroke counts for testing. This allows tests to re-use the same
   * toolbar.
   */
  resetStrokesForTesting() {
    this.currentStroke = 0;
    this.mostRecentStroke = 0;
    this.updateCanUndoRedo_();
    this.dispatchEvent(new CustomEvent(
        'strokes-updated', {detail: 0, bubbles: true, composed: true}));
  }
  // </if>

  protected isFormFieldFocused_(): boolean {
    return this.formFieldFocus !== FormFieldFocusType.NONE;
  }

  /**
   * Updates the toolbar's presentation mode available flag depending on current
   * conditions.
   */
  protected presentationModeAvailable_(): boolean {
    // <if expr="enable_ink">
    return !this.annotationMode && !this.embeddedViewer;
    // </if>
    // <if expr="not enable_ink">
    return !this.embeddedViewer;
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-toolbar': ViewerToolbarElement;
  }
}

customElements.define(ViewerToolbarElement.is, ViewerToolbarElement);
