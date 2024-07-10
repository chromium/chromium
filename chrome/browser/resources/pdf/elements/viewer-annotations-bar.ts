// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './icons.html.js';
import './pdf-shared.css.js';
import './viewer-pen-options.js';
import './viewer-toolbar-dropdown.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {AnnotationTool} from '../annotation_tool.js';
// <if expr="enable_pdf_ink2">
import type {AnnotationBrush, AnnotationBrushType} from '../constants.js';
import {PluginController, PluginControllerEventType} from '../controller.js';
// </if>
// <if expr="enable_ink">
import {InkController, InkControllerEventType} from '../ink_controller.js';
// </if>

import {getTemplate} from './viewer-annotations-bar.html.js';
import type {ViewerPenOptionsElement} from './viewer-pen-options.js';
import type {ViewerToolbarDropdownElement} from './viewer-toolbar-dropdown.js';

export interface ViewerAnnotationsBarElement {
  $: {
    eraser: HTMLElement,
    highlighter: HTMLElement,
    pen: HTMLElement,
    redo: CrIconButtonElement,
    undo: CrIconButtonElement,
  };
}

export class ViewerAnnotationsBarElement extends PolymerElement {
  static get is() {
    return 'viewer-annotations-bar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      annotationMode: {
        type: Boolean,
        observer: 'onAnnotationModeChanged_',
      },

      annotationTool_: Object,

      canUndoAnnotation_: {
        type: Boolean,
        value: false,
      },

      canRedoAnnotation_: {
        type: Boolean,
        value: false,
      },

      // <if expr="enable_pdf_ink2">
      pdfInk2Enabled: Boolean,
      // </if>
    };
  }

  annotationMode: boolean;
  private annotationTool_: AnnotationTool|null;
  private canUndoAnnotation_: boolean;
  private canRedoAnnotation_: boolean;
  // <if expr="enable_ink">
  private inkController_: InkController = InkController.getInstance();
  // </if>
  private tracker_: EventTracker = new EventTracker();
  // <if expr="enable_pdf_ink2">
  pdfInk2Enabled: boolean;
  private pluginController_: PluginController = PluginController.getInstance();
  private currentStroke: number = 0;
  private mostRecentStroke: number = 0;
  // </if>

  constructor() {
    super();

    // <if expr="enable_ink">
    this.tracker_.add(
        this.inkController_.getEventTarget(),
        InkControllerEventType.SET_ANNOTATION_UNDO_STATE,
        this.setAnnotationUndoState_.bind(this));

    this.tracker_.add(
        this.inkController_.getEventTarget(), InkControllerEventType.LOADED,
        () => {
          if (this.annotationTool_) {
            assert(this.inkController_.isActive);
            this.inkController_.setAnnotationTool(this.annotationTool_);
          }
        });
    // </if>

    // <if expr="enable_pdf_ink2">
    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.FINISH_INK_STROKE,
        this.handleFinishInkStroke_.bind(this));
    // </if>
  }

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
  // </if>

  // <if expr="enable_ink">
  private setAnnotationUndoState_(
      e: CustomEvent<{canUndo: boolean, canRedo: boolean}>) {
    this.canUndoAnnotation_ = e.detail.canUndo;
    this.canRedoAnnotation_ = e.detail.canRedo;
  }
  // </if>

  private onUndoClick_() {
    // <if expr="enable_pdf_ink2">
    if (this.pdfInk2Enabled) {
      assert(this.currentStroke > 0);
      this.pluginController_.undo();
      this.currentStroke--;

      this.canUndoAnnotation_ = this.currentStroke > 0;
      if (!this.canUndoAnnotation_) {
        this.dispatchEvent(new CustomEvent(
            'can-undo-changed',
            {detail: false, bubbles: true, composed: true}));
      }
      this.canRedoAnnotation_ = true;
      return;
    }
    // </if>

    // <if expr="enable_ink">
    this.inkController_.undo();
    // </if>
  }

  private onRedoClick_() {
    // <if expr="enable_pdf_ink2">
    if (this.pdfInk2Enabled) {
      assert(this.currentStroke < this.mostRecentStroke);
      this.pluginController_.redo();
      this.currentStroke++;

      if (!this.canUndoAnnotation_) {
        this.canUndoAnnotation_ = true;
        this.dispatchEvent(new CustomEvent(
            'can-undo-changed', {detail: true, bubbles: true, composed: true}));
      }
      this.canRedoAnnotation_ = this.currentStroke < this.mostRecentStroke;
      return;
    }
    // </if>

    // <if expr="enable_ink">
    this.inkController_.redo();
    // </if>
  }

  private onAnnotationModeChanged_() {
    if (this.annotationMode) {
      // Select pen tool when entering annotation mode.
      this.updateAnnotationTool_(this.shadowRoot!.querySelector('#pen')!);
    }
  }

  private annotationToolClicked_(e: Event) {
    this.updateAnnotationTool_(e.currentTarget as HTMLElement);
  }

  private annotationToolOptionChanged_(e: Event) {
    const element = (e.currentTarget as HTMLElement).parentElement!;
    if (!this.annotationTool_ || element.id !== this.annotationTool_.tool) {
      return;
    }
    this.updateAnnotationTool_(element);
  }

  private updateAnnotationTool_(element: HTMLElement) {
    const tool = element.id;
    const options =
        element.querySelector<ViewerPenOptionsElement>('viewer-pen-options') ||
        {
          selectedSize: 1,
          selectedColor: null,
        };

    element.style.setProperty('--pen-tip-fill', options.selectedColor);
    element.style.setProperty(
        '--pen-tip-border',
        options.selectedColor === '#000000' ? 'currentcolor' :
                                              options.selectedColor);
    const newAnnotationTool: AnnotationTool = {
      tool: tool,
      size: options.selectedSize,
      color: options.selectedColor ? options.selectedColor : undefined,
    };
    // <if expr="enable_pdf_ink2">
    if (this.pdfInk2Enabled) {
      // Only set the annotation brush and `this.annotationTool_` if the values
      // have changed.
      if (this.isNewAnnotationTool_(newAnnotationTool)) {
        this.pluginController_.setAnnotationBrush(
            this.getAnnotationBrush_(newAnnotationTool));
        this.annotationTool_ = newAnnotationTool;
      }
      return;
    }
    // </if>

    this.annotationTool_ = newAnnotationTool;

    // <if expr="enable_ink">
    this.inkController_.setAnnotationTool(this.annotationTool_);
    // </if>
  }

  // <if expr="enable_pdf_ink2">
  /**
   * Returns whether `annotationTool` is different from `this.annotationTool_`.
   * @param annotationTool The `AnnotationTool` to check.
   * @returns True if `this.annotationTool_` is null or doesn't have the same
   *     values as `annotationTool`, false otherwise.
   */
  private isNewAnnotationTool_(annotationTool: AnnotationTool): boolean {
    if (!this.annotationTool_) {
      return true;
    }

    return this.annotationTool_.tool !== annotationTool.tool ||
        this.annotationTool_.size !== annotationTool.size ||
        this.annotationTool_.color !== annotationTool.color;
  }

  /**
   * @return The `AnnotationBrush` constructed using the values in
   *     `annotationTool`.
   */
  private getAnnotationBrush_(annotationTool: AnnotationTool): AnnotationBrush {
    const brush: AnnotationBrush = {
      type: annotationTool.tool as AnnotationBrushType,
      size: annotationTool.size,
    };

    if (annotationTool.color !== undefined) {
      // `AnnotationTool`'s color is a hex-coded color string, formatted as
      // '#ffffff'.
      const hexColor = annotationTool.color;
      assert(/^#[0-9a-f]{6}$/.test(hexColor));

      brush.color = {
        r: parseInt(hexColor.substring(1, 3), 16),
        g: parseInt(hexColor.substring(3, 5), 16),
        b: parseInt(hexColor.substring(5, 7), 16),
      };
    }

    return brush;
  }
  // </if>

  /** @return Whether the annotation tool is using tool `toolName`. */
  private isAnnotationTool_(toolName: string): boolean {
    return !!this.annotationTool_ && this.annotationTool_.tool === toolName;
  }

  private getOpenDropdowns_(): NodeListOf<ViewerToolbarDropdownElement> {
    return this.shadowRoot!.querySelectorAll(
        'viewer-toolbar-dropdown[dropdown-open]');
  }

  /** @return Whether one of the dropdowns is open. */
  hasOpenDropdown(): boolean {
    return this.getOpenDropdowns_().length > 0;
  }

  closeDropdowns() {
    this.getOpenDropdowns_().forEach(element => element.toggleDropdown());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-annotations-bar': ViewerAnnotationsBarElement;
  }
}

customElements.define(
    ViewerAnnotationsBarElement.is, ViewerAnnotationsBarElement);
