// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';
import './icons.html.js';
import './viewer_download_controls.js';
import './viewer_page_selector.js';
// <if expr="enable_pdf_save_to_drive">
import './viewer_save_to_drive_controls.js';
// </if>
import './shared_vars.css.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
// <if expr="enable_pdf_ink2 or enable_pdf_save_to_drive">
import {assert} from 'chrome://resources/js/assert.js';
// </if>
// <if expr="enable_pdf_ink2">
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
// </if>
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {LoadTimeDataRaw} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// <if expr="enable_pdf_ink2">
import {AnnotationMode} from '../constants.js';
// </if>
import {FittingType, FormFieldFocusType} from '../constants.js';
// <if expr="enable_pdf_save_to_drive">
import {SaveToDriveState} from '../constants.js';
// </if>
// <if expr="enable_pdf_ink2">
import {PluginController, PluginControllerEventType} from '../controller.js';
// </if>
import {record, UserAction} from '../metrics.js';

import {getCss} from './viewer_toolbar.css.js';
import {getHtml} from './viewer_toolbar.html.js';

declare global {
  interface HTMLElementEventMap {
    // <if expr="enable_pdf_ink2">
    'annotation-mode-updated': CustomEvent<AnnotationMode>;
    // </if>
    'display-annotations-changed': CustomEvent<boolean>;
    'fit-to-changed': CustomEvent<FittingType>;
  }
}

export interface ViewerToolbarElement {
  $: {
    sidenavToggle: HTMLButtonElement,
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
      docTitle: {type: String},
      docLength: {type: Number},
      embeddedViewer: {type: Boolean},
      hasEdits: {type: Boolean},
      hasEnteredAnnotationMode: {type: Boolean},
      formFieldFocus: {type: String},
      loadProgress: {type: Number},

      loading_: {
        type: Boolean,
        reflect: true,
      },

      pageNo: {type: Number},

      rotated: {type: Boolean},
      strings: {type: Object},
      viewportZoom: {type: Number},
      zoomBounds: {type: Object},
      sidenavCollapsed: {type: Boolean},
      twoUpViewEnabled: {type: Boolean},

      moreMenuOpen_: {
        type: Boolean,
        reflect: true,
      },

      displayAnnotations_: {type: Boolean},
      fittingType_: {type: Number},
      printingEnabled_: {type: Boolean},
      viewportZoomPercent_: {type: Number},

      // <if expr="enable_pdf_ink2">
      annotationAvailable: {type: Boolean},
      annotationMode: {
        type: String,
        reflect: true,
      },
      enableUndoRedo: {type: Boolean},
      hasInk2Edits: {type: Boolean},
      pdfInk2Enabled: {type: Boolean},
      canRedoAnnotation_: {type: Boolean},
      canUndoAnnotation_: {type: Boolean},
      pdfTextAnnotationsEnabled_: {type: Boolean},
      // </if> enable_pdf_ink2

      // <if expr="enable_pdf_save_to_drive">
      pdfSaveToDriveEnabled: {type: Boolean},
      saveToDriveProgress: {type: Number},
      saveToDriveState: {type: String},
      // </if> enable_pdf_save_to_drive
    };
  }

  accessor docTitle: string = '';
  accessor docLength: number = 0;
  accessor embeddedViewer: boolean = false;
  accessor hasEdits: boolean = false;
  accessor hasEnteredAnnotationMode: boolean = false;
  accessor formFieldFocus: FormFieldFocusType = FormFieldFocusType.NONE;
  accessor loadProgress: number = 0;
  accessor pageNo: number = 0;
  accessor rotated: boolean = false;
  accessor strings: LoadTimeDataRaw|undefined;
  accessor viewportZoom: number = 0;
  accessor zoomBounds: {min: number, max: number} = {min: 0, max: 0};
  accessor sidenavCollapsed: boolean = false;
  accessor twoUpViewEnabled: boolean = false;
  protected accessor displayAnnotations_: boolean = true;
  private accessor fittingType_: FittingType = FittingType.FIT_TO_PAGE;
  protected accessor moreMenuOpen_: boolean = false;
  protected accessor loading_: boolean = true;
  protected accessor printingEnabled_: boolean = false;
  private accessor viewportZoomPercent_: number = 0;

  // <if expr="enable_pdf_save_to_drive">
  accessor pdfSaveToDriveEnabled: boolean = false;
  accessor saveToDriveProgress: number = 0;
  accessor saveToDriveState: SaveToDriveState = SaveToDriveState.UNINITIALIZED;
  // </if> enable_pdf_save_to_drive

  // <if expr="enable_pdf_ink2">
  // Ink2 reactive properties
  accessor annotationAvailable: boolean = false;
  accessor annotationMode: AnnotationMode = AnnotationMode.OFF;
  accessor enableUndoRedo: boolean = true;
  accessor hasInk2Edits: boolean = false;
  accessor pdfInk2Enabled: boolean = false;
  protected accessor canRedoAnnotation_: boolean = false;
  protected accessor canUndoAnnotation_: boolean = false;
  protected accessor pdfTextAnnotationsEnabled_: boolean = false;

  // Ink2 class members
  private currentStroke: number = 0;
  private mostRecentStroke: number = 0;
  private pluginController_: PluginController = PluginController.getInstance();
  private strokeInProgress_: boolean = false;
  private tracker_: EventTracker = new EventTracker();

  constructor() {
    super();

    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.FINISH_INK_STROKE,
        this.handleFinishInkStroke_.bind(this));
    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.START_INK_STROKE,
        this.handleStartInkStroke_.bind(this));
  }
  // </if> enable_pdf_ink2

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('loadProgress')) {
      this.loading_ = this.loadProgress < 100;
    }

    if (changedProperties.has('strings') && this.strings) {
      this.updateLoadTimeData_();
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

  private updateLoadTimeData_() {
    this.printingEnabled_ = loadTimeData.getBoolean('printingEnabled');
    // <if expr="enable_pdf_ink2">
    this.pdfTextAnnotationsEnabled_ =
        loadTimeData.getBoolean('pdfTextAnnotationsEnabled');
    // </if>
  }

  protected onSidenavToggleClick_() {
    record(UserAction.TOGGLE_SIDENAV);
    this.dispatchEvent(new CustomEvent('sidenav-toggle-click'));
  }

  protected fitToButtonIcon_(): string {
    return 'pdf' +
        (this.fittingType_ === FittingType.FIT_TO_PAGE ? ':fit-to-height' :
                                                         ':fit-to-width');
  }

  /** @return The appropriate tooltip for the current state. */
  protected getFitToButtonTooltip_() {
    if (!this.strings) {
      return '';
    }
    return loadTimeData.getString(
        this.fittingType_ === FittingType.FIT_TO_PAGE ? 'tooltipFitToPage' :
                                                        'tooltipFitToWidth');
  }

  // <if expr="enable_pdf_ink2">
  protected showInk2Buttons_(): boolean {
    return this.pdfInk2Enabled;
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
    return this.shadowRoot.querySelector('#zoom-controls input')!;
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
    const anchor = this.shadowRoot.querySelector<HTMLElement>('#more')!;
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

  // <if expr="enable_pdf_ink2">
  // Gets a CSS class of "active" if `mode` is the active annotation mode.
  protected getActive_(mode: AnnotationMode): string {
    return mode === this.annotationMode ? 'active' : '';
  }

  // Returns true if the button is toggled on, false otherwise.
  protected getAriaPressed_(mode: AnnotationMode): string {
    return mode === this.annotationMode ? 'true' : 'false';
  }

  protected onAnnotationClick_() {
    assert(this.pdfInk2Enabled);

    const newAnnotationMode = this.annotationMode === AnnotationMode.DRAW ?
        AnnotationMode.OFF :
        AnnotationMode.DRAW;
    this.setAnnotationMode(newAnnotationMode);
  }

  setAnnotationMode(annotationMode: AnnotationMode) {
    assert(this.pdfInk2Enabled);

    this.dispatchEvent(
        new CustomEvent('annotation-mode-updated', {detail: annotationMode}));
  }
  // </if> enable_pdf_ink2

  // <if expr="enable_pdf_ink2">
  protected onTextAnnotationClick_() {
    this.setAnnotationMode(
        this.annotationMode === AnnotationMode.TEXT ? AnnotationMode.OFF :
                                                      AnnotationMode.TEXT);
  }

  /**
   * Handles when the user starts a stroke. While the stroke is in progress,
   * disallow undo/redo operations.
   */
  private handleStartInkStroke_() {
    this.strokeInProgress_ = true;
  }

  /**
   * Handles whether the undo and redo buttons should be enabled or disabled
   * when a new Ink stroke is added to or erased from the page. This event
   * fires when stroking finishes, but not all strokes (e.g. eraser strokes)
   * actually modify the page.
   */
  private handleFinishInkStroke_(e: CustomEvent<boolean>) {
    const modified = e.detail;
    if (modified) {
      this.currentStroke++;
      this.mostRecentStroke = this.currentStroke;

      // When a new stroke modification occurs, it can always be undone. Since
      // it's the most recent modification, the redo action cannot be performed.
      this.canUndoAnnotation_ = true;
      this.canRedoAnnotation_ = false;
    }

    this.strokeInProgress_ = false;
  }

  protected computeEnableUndo_(): boolean {
    return this.canUndoAnnotation_ && !this.strokeInProgress_ &&
        this.enableUndoRedo;
  }

  protected computeEnableRedo_(): boolean {
    return this.canRedoAnnotation_ && !this.strokeInProgress_ &&
        this.enableUndoRedo;
  }

  /**
   * Undo an annotation stroke, if possible.
   */
  undo() {
    if (!this.computeEnableUndo_()) {
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
    record(UserAction.UNDO_INK2);
  }

  /**
   * Redo an annotation stroke, if possible.
   */
  redo() {
    if (!this.computeEnableRedo_()) {
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
    record(UserAction.REDO_INK2);
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
    return !this.embeddedViewer;
  }

  // <if expr="enable_pdf_save_to_drive">
  getSaveToDriveBubbleAnchor(): HTMLElement {
    const anchor = this.shadowRoot.querySelector<HTMLElement>('#save-to-drive');
    assert(anchor);
    return anchor;
  }
  // </if> enable_pdf_save_to_drive
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-toolbar': ViewerToolbarElement;
  }
}

customElements.define(ViewerToolbarElement.is, ViewerToolbarElement);
