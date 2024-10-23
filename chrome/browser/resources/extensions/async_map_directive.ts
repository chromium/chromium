// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {AsyncDirective, directive, html, noChange, PartType} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {ChildPart, DirectiveParameters, PartInfo, TemplateResult} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// Directive to render some items in an array asynchronously. Initially
// renders `initialCount` items, and renders remaining items asynchronously
// in chunking mode, where each chunk is rendered on a subsequent animation
// frame. Chunk size is initialized to `initialCount` and increases by
// `initialCount` when frames render more quickly than the target, and halves if
// frames render more slowly than the target (20fps).
// Also supports passing a filter function, to only render items in the array
// that match the filter (i.e. items for which filter(item) === true).
// Dispatches a 'rendered-items-changed' event, with a `detail` property set
// to the total number of rendered items, each time the rendered items are
// updated.
class AsyncMapDirective<T> extends AsyncDirective {
  template: (item: T) => TemplateResult = _item => html``;
  initialCount: number = -1;
  items: T[] = [];
  filter: ((item: T) => boolean)|null = null;

  private chunkSize_: number = -1;
  private filteredItems_: T[] = [];
  private renderedItems_: T[] = [];
  private renderStartTime_: number = 0;
  private targetElapsedTime_: number = 50;  // 20fps
  private eventTarget_: EventTarget|null = null;
  private requestId_: number|null = null;

  constructor(partInfo: PartInfo) {
    super(partInfo);

    assert(
        partInfo.type === PartType.CHILD,
        'asyncMap() can only be used in text expressions');
  }

  override update(part: ChildPart, [
    items,
    template,
    initialCount,
    filter,
  ]: DirectiveParameters<this>) {
    this.eventTarget_ = part.parentNode instanceof ShadowRoot ?
        part.parentNode.host :
        part.parentNode;
    return this.render(items, template, initialCount, filter);
  }

  render(
      items: T[], template: ((item: T) => TemplateResult), initialCount: number,
      filter: (((item: T) => boolean)|null) = null) {
    if (items === this.items && filter === this.filter) {
      return noChange;
    }

    if (!this.isConnected) {
      return html``;
    }

    this.template = template;
    this.items = items;
    this.filter = filter;
    this.filteredItems_ = filter ? items.filter(i => filter(i)) : items;
    assert(initialCount > 0);
    this.initialCount = initialCount;
    if (this.chunkSize_ === -1) {
      this.chunkSize_ = this.initialCount;
    }
    this.renderedItems_ = this.filteredItems_.slice(0, this.initialCount);
    const initialRender = this.renderItems_();
    this.renderInChunks_();
    return initialRender;
  }

  private renderItems_(): TemplateResult {
    // Notify interested parties. Async so that rendering the new items is
    // done before the event is fired.
    const numItems = this.renderedItems_.length;
    setTimeout(() => {
      if (this.eventTarget_) {
        this.eventTarget_.dispatchEvent(new CustomEvent(
            'rendered-items-changed',
            {bubbles: true, composed: true, detail: numItems}));
      }
    }, 0);
    this.renderStartTime_ = performance.now();
    return html`${this.renderedItems_.map(item => this.template(item))}`;
  }

  private async renderInChunks_() {
    let length = this.renderedItems_.length;
    const arrayRef = this.filteredItems_;
    while (length < arrayRef.length) {
      await new Promise<void>((resolve) => {
        this.requestId_ = requestAnimationFrame(() => {
          if (this.requestId_) {
            cancelAnimationFrame(this.requestId_);
          }
          resolve();
        });
      });

      if (!this.isConnected || this.filteredItems_ !== arrayRef) {
        return;  // value updated, no longer our loop
      }

      const elapsed = performance.now() - this.renderStartTime_;

      // Additive increase, multiplicative decrease
      if (elapsed < this.targetElapsedTime_) {
        this.chunkSize_ += this.initialCount;
      } else {
        this.chunkSize_ = Math.max(1, Math.floor(this.chunkSize_ / 2));
      }

      const newLength = Math.min(length + this.chunkSize_, arrayRef.length);
      this.renderedItems_.push(...this.filteredItems_.slice(length, newLength));
      length = newLength;
      this.setValue(this.renderItems_());
    }
  }
}

export interface AsyncMapDirectiveFn {
  <T>(
      items: T[],
      template: (item: T) => TemplateResult,
      initialCount: number,
      filter?: ((item: T) => boolean)|null,
      ): unknown;
}

export const asyncMap = directive(AsyncMapDirective) as AsyncMapDirectiveFn;
