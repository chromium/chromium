// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {AsyncDirective, directive, html, PartType} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PartInfo, TemplateResult} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// Directive to render some items in an array asynchronously. Initially
// renders `initialCount` items, and renders remaining items asynchronously
// in chunking mode, where each chunk is rendered on a subsequent animation
// frame. Chunk size is initialized to `initialCount` and increases by
// `initialCount` when frames render more quickly than the target, and halves if
// frames render more slowly than the target (20fps).
class AsyncMapDirective<T> extends AsyncDirective {
  template: (item: T) => TemplateResult = _item => html``;
  initialCount: number = -1;
  items: T[] = [];

  private chunkSize_: number = -1;
  private renderedItems_: T[] = [];
  private renderStartTime_: number = 0;
  private targetElapsedTime_: number = 50;  // 20fps
  private requestId_: number|null = null;
  private timeout_: number|null = null;

  constructor(partInfo: PartInfo) {
    super(partInfo);

    assert(
        partInfo.type === PartType.CHILD,
        'asyncMap() can only be used in text expressions');
  }

  render(
      items: T[], template: ((item: T) => TemplateResult),
      initialCount: number) {
    // Clear any outstanding timeout or animation frame.
    if (this.timeout_) {
      clearTimeout(this.timeout_);
      this.timeout_ = null;
    }
    if (this.requestId_) {
      cancelAnimationFrame(this.requestId_);
      this.requestId_ = null;
    }

    this.renderStartTime_ = 0;
    this.template = template;
    this.items = items;
    assert(initialCount > 0);
    this.initialCount = initialCount;
    if (this.chunkSize_ === -1) {
      this.chunkSize_ = this.initialCount;
    }

    // Don't unnecessarily fully remove items. This will create a larger
    // change in the template. Instead, update the number of items already
    // rendered + the current chunk size initially.
    const count =
        Math.min(items.length, this.renderedItems_.length + this.chunkSize_);
    this.renderedItems_ = this.items.slice(0, count);
    if (count < items.length) {
      this.timeout_ = setTimeout(() => this.renderInChunks_(), 0);
    }
    return this.renderItems_();
  }

  private renderItems_(): TemplateResult[] {
    return this.renderedItems_.map(item => this.template(item));
  }

  private async renderInChunks_() {
    this.timeout_ = null;

    let length = this.renderedItems_.length;
    const arrayRef = this.items;
    while (length < arrayRef.length) {
      await new Promise<void>((resolve) => {
        this.requestId_ = requestAnimationFrame(() => {
          if (this.requestId_) {
            cancelAnimationFrame(this.requestId_);
            this.requestId_ = null;
          }
          resolve();
        });
      });

      if (this.items !== arrayRef) {
        return;  // value updated, no longer our loop
      }

      // Adjust the chunk size if needed.
      if (this.renderStartTime_ > 0) {
        const elapsed = performance.now() - this.renderStartTime_;

        // Additive increase, multiplicative decrease
        if (elapsed < this.targetElapsedTime_) {
          this.chunkSize_ += this.initialCount;
        } else {
          this.chunkSize_ =
              Math.max(this.initialCount, Math.floor(this.chunkSize_ / 2));
        }
      }

      const newLength = Math.min(length + this.chunkSize_, arrayRef.length);
      this.renderedItems_.push(...this.items.slice(length, newLength));
      length = newLength;
      this.renderStartTime_ = performance.now();
      this.setValue(this.renderItems_());
    }
  }
}

export interface AsyncMapDirectiveFn {
  <T>(
      items: T[],
      template: (item: T) => TemplateResult,
      initialCount: number,
      ): unknown;
}

export const asyncMap = directive(AsyncMapDirective) as AsyncMapDirectiveFn;
