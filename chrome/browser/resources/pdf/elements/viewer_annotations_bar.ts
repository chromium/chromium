// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './icons.html.js';
import './viewer_pen_options.js';
import './viewer_toolbar_dropdown.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {AnnotationTool} from '../annotation_tool.js';
import {InkController, InkControllerEventType} from '../ink_controller.js';

import {getTemplate} from './viewer_annotations_bar.html.js';
import type {ViewerPenOptionsElement} from './viewer_pen_options.js';
import type {ViewerToolbarDropdownElement} from './viewer_toolbar_dropdown.js';

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
    };
  }

  annotationMode: boolean;
  private annotationTool_: AnnotationTool|null;
  private canUndoAnnotation_: boolean;
  private canRedoAnnotation_: boolean;
  private inkController_: InkController = InkController.getInstance();
  private tracker_: EventTracker = new EventTracker();

  constructor() {
    super();

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
  }

  private setAnnotationUndoState_(
      e: CustomEvent<{canUndo: boolean, canRedo: boolean}>) {
    this.canUndoAnnotation_ = e.detail.canUndo;
    this.canRedoAnnotation_ = e.detail.canRedo;
  }

  private onUndoClick_() {
    this.inkController_.undo();
  }

  private onRedoClick_() {
    this.inkController_.redo();
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
    this.annotationTool_ = newAnnotationTool;

    this.inkController_.setAnnotationTool(this.annotationTool_);
  }

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
