// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {DragWrapper} from 'chrome://resources/js/drag_wrapper.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {DragAndDropHandler} from './drag_and_drop_handler.js';
import {getCss} from './drop_overlay.css.js';
import {getHtml} from './drop_overlay.html.js';

export class ExtensionsDropOverlayElement extends CrLitElement {
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

  accessor dragEnabled: boolean = false;
  private dragWrapperHandler_: DragAndDropHandler;
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();

    this.hidden = true;
    this.dragWrapperHandler_ =
        new DragAndDropHandler(true, document.documentElement);
  }

  override connectedCallback() {
    super.connectedCallback();

    const dragTarget = document.documentElement;
    this.eventTracker_.add(dragTarget, 'extension-drag-started', () => {
      this.hidden = false;
    });

    this.eventTracker_.add(dragTarget, 'extension-drag-ended', () => {
      this.hidden = true;
    });

    this.eventTracker_.add(
        dragTarget, 'drag-and-drop-load-error', (e: CustomEvent) => {
          this.fire('load-error', e.detail);
        });

    new DragWrapper(dragTarget, this.dragWrapperHandler_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('dragEnabled')) {
      this.dragWrapperHandler_.dragEnabled = this.dragEnabled;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-drop-overlay': ExtensionsDropOverlayElement;
  }
}

customElements.define(
    ExtensionsDropOverlayElement.is, ExtensionsDropOverlayElement);
