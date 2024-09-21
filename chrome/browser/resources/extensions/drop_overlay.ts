// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {DragWrapper} from 'chrome://resources/js/drag_wrapper.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {DragAndDropHandler} from './drag_and_drop_handler.js';
import {getCss} from './drop_overlay.css.js';
import {getHtml} from './drop_overlay.html.js';

// TODO (rbpotter): Rename back to ExtensionsDropOverlayElement when .html.ts
// files are checked in.
export class DropOverlayElement extends CrLitElement {
  static get is() {
    return 'extensions-drop-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dragEnabled: {type: Boolean},
    };
  }

  dragEnabled: boolean = false;
  private dragWrapperHandler_: DragAndDropHandler;
  private dragWrapper_: DragWrapper;

  constructor() {
    super();

    this.hidden = true;
    const dragTarget = document.documentElement;
    this.dragWrapperHandler_ = new DragAndDropHandler(true, dragTarget);
    // TODO(devlin): All these dragTarget listeners leak (they aren't removed
    // when the element is). This only matters in tests at the moment, but would
    // be good to fix.
    dragTarget.addEventListener('extension-drag-started', () => {
      this.hidden = false;
    });
    dragTarget.addEventListener('extension-drag-ended', () => {
      this.hidden = true;
    });
    dragTarget.addEventListener('drag-and-drop-load-error', (e) => {
      this.dispatchEvent(new CustomEvent(
          'load-error', {bubbles: true, composed: true, detail: e.detail}));
    });
    this.dragWrapper_ = new DragWrapper(dragTarget, this.dragWrapperHandler_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('dragEnabled')) {
      this.dragWrapperHandler_.dragEnabled = this.dragEnabled;
    }
  }
}

customElements.define(DropOverlayElement.is, DropOverlayElement);
