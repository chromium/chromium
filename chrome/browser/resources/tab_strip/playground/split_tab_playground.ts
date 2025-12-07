// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './split_tab_playground.html.js';
import type {TabElement} from './tab_playground.js';
import {isTabElement} from './tab_playground.js';

export class SplitTabElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private container_: HTMLElement;
  private slot_: HTMLSlotElement;
  private dragHandler_:
      (element: SplitTabElement, x: number, y: number) => void = () => {};

  constructor() {
    super();

    this.container_ = this.getRequiredElement('#container');
    this.container_.addEventListener(
        'dragend',
        (event: MouseEvent) =>
            this.dragHandler_(this, event.clientX, event.clientY));

    this.slot_ = this.getRequiredElement<HTMLSlotElement>('#slot');
    this.slot_.addEventListener('slotchange', () => this.onSlotChange_());
  }

  set dragEndHandler(
      handler: (element: SplitTabElement, x: number, y: number) => void) {
    this.dragHandler_ = handler;
  }

  private onSlotChange_() {
    const assignedElements = this.slot_.assignedElements();
    for (const element of assignedElements) {
      if (isTabElement(element)) {
        (element as TabElement).draggable = false;
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-split-tab-playground': SplitTabElement;
  }
}

customElements.define('tabstrip-split-tab-playground', SplitTabElement);

export function isSplitTabElement(element: Element): boolean {
  return element.tagName === 'TABSTRIP-SPLIT-TAB-PLAYGROUND';
}
