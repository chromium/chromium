// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './post_selection_renderer.html.js';

// Bounding box send to PostSelectionRendererElement to render a bounding box.
// The numbers should be normalized to the image dimensions, between 0 and 1
export interface PostSelectionBoundingBox {
  top: number;
  left: number;
  width: number;
  height: number;
}

// Takes the value between 0-1 and returns a string in the from '__%';
// TODO(b/333620724): Move to a separate file and reuse across codebase.
function toPercent(value: number): string {
  return `${value * 100}%`;
}

export interface PostSelectionRendererElement {
  $: {
    postSelection: HTMLElement,
    selectionCorners: HTMLElement,
  };
}

/*
 * Renders the users visual selection after one is made. This element is also
 * responsible for allowing the user to adjust their region to issue a new
 * Lens request.
 */
export class PostSelectionRendererElement extends PolymerElement {
  static get is() {
    return 'post-selection-renderer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      top: Number,
      left: Number,
      height: Number,
      width: Number,
    };
  }

  private eventTracker_: EventTracker = new EventTracker();
  // The bounds of the current selection
  private top: number = 0;
  private left: number = 0;
  private height: number = 0;
  private width: number = 0;

  constructor() {
    super();

    // Setup CSS Houdini API
    CSS.paintWorklet.addModule('post_selection_paint_worklet.js');
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'render-post-selection',
        (e: CustomEvent<PostSelectionBoundingBox>) => {
          this.onRenderPostSelection(e);
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  clearSelection() {
    this.height = 0;
    this.width = 0;
  }

  private onRenderPostSelection(e: CustomEvent<PostSelectionBoundingBox>) {
    this.top = e.detail.top;
    this.left = e.detail.left;
    this.height = e.detail.height;
    this.width = e.detail.width;

    // Set the CSS properties to reflect these bounds.
    this.style.setProperty('--selection-width', toPercent(this.width));
    this.style.setProperty('--selection-height', toPercent(this.height));
    this.style.setProperty('--selection-top', toPercent(this.top));
    this.style.setProperty('--selection-left', toPercent(this.left));
  }

  private hasSelection(): boolean {
    return this.width > 0 && this.height > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'post-selection-renderer': PostSelectionRendererElement;
  }
}

customElements.define(
    PostSelectionRendererElement.is, PostSelectionRendererElement);
