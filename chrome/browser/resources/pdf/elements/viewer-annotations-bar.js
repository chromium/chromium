// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './icons.js';
import './shared-css.js';
import './viewer-pen-options.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InkController, InkControllerEventType} from '../ink_controller.js';
import {ViewerToolbarDropdownElement} from './viewer-toolbar-dropdown.js';

export class ViewerAnnotationsBarElement extends PolymerElement {
  static get is() {
    return 'viewer-annotations-bar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      annotationMode: {
        type: Boolean,
        observer: 'onAnnotationModeChanged_',
      },

      /** @private {?AnnotationTool} */
      annotationTool_: Object,

      /** @private */
      canUndoAnnotation_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      canRedoAnnotation_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /** @private {!InkController} */
    this.inkController_ = InkController.getInstance();

    /** @private {!EventTracker} */
    this.tracker_ = new EventTracker();

    this.tracker_.add(
        this.inkController_.getEventTarget(),
        InkControllerEventType.SET_ANNOTATION_UNDO_STATE,
        e => this.setAnnotationUndoState_(e));

    this.tracker_.add(
        this.inkController_.getEventTarget(), InkControllerEventType.LOADED,
        () => {
          if (this.annotationTool_) {
            assert(this.inkController_.isActive);
            this.inkController_.setAnnotationTool(this.annotationTool_);
          }
        });
  }

  /**
   * @param {!CustomEvent<{canUndo: boolean, canRedo: boolean}>} e
   * @private
   */
  setAnnotationUndoState_(e) {
    this.canUndoAnnotation_ = e.detail.canUndo;
    this.canRedoAnnotation_ = e.detail.canRedo;
  }

  /** @private */
  onUndoClick_() {
    this.inkController_.undo();
  }

  /** @private */
  onRedoClick_() {
    this.inkController_.redo();
  }

  /** @private */
  onAnnotationModeChanged_() {
    if (this.annotationMode) {
      // Select pen tool when entering annotation mode.
      this.updateAnnotationTool_(/** @type {!ViewerToolbarDropdownElement} */ (
          this.shadowRoot.querySelector('#pen')));
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  annotationToolClicked_(e) {
    this.updateAnnotationTool_(/** @type {!HTMLElement} */ (e.currentTarget));
  }

  /**
   * @param {!Event} e
   * @private
   */
  annotationToolOptionChanged_(e) {
    const element = e.currentTarget.parentElement;
    if (!this.annotationTool_ || element.id !== this.annotationTool_.tool) {
      return;
    }
    this.updateAnnotationTool_(e.currentTarget.parentElement);
  }

  /**
   * @param {!HTMLElement} element
   * @private
   */
  updateAnnotationTool_(element) {
    const tool = element.id;
    const options = element.querySelector('viewer-pen-options') || {
      selectedSize: 1,
      selectedColor: undefined,
    };
    const attributeStyleMap = element.attributeStyleMap;
    attributeStyleMap.set('--pen-tip-fill', options.selectedColor);
    attributeStyleMap.set(
        '--pen-tip-border',
        options.selectedColor === '#000000' ? 'currentcolor' :
                                              options.selectedColor);
    this.annotationTool_ = {
      tool: tool,
      size: options.selectedSize,
      color: options.selectedColor,
    };
    this.inkController_.setAnnotationTool(this.annotationTool_);
  }

  /**
   * @param {string} toolName
   * @return {boolean} Whether the annotation tool is using tool |toolName|.
   * @private
   */
  isAnnotationTool_(toolName) {
    return !!this.annotationTool_ && this.annotationTool_.tool === toolName;
  }

  /** @return {!NodeList<!ViewerToolbarDropdownElement>} */
  getOpenDropdowns_() {
    return /** @type {!NodeList<!ViewerToolbarDropdownElement>} */ (
        this.shadowRoot.querySelectorAll(
            'viewer-toolbar-dropdown[dropdown-open]'));
  }

  /** @return {boolean} Whether one of the dropdowns is open. */
  hasOpenDropdown() {
    return this.getOpenDropdowns_().length > 0;
  }

  closeDropdowns() {
    this.getOpenDropdowns_().forEach(element => element.toggleDropdown());
  }
}
customElements.define(
    ViewerAnnotationsBarElement.is, ViewerAnnotationsBarElement);
