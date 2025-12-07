// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from '../tab_group.html.js';
import type {TabGroupVisualData} from '../tab_strip.mojom-webui.js';

export class TabGroupElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private chip_: HTMLElement;
  private dragHandler_: any;

  constructor() {
    super();

    this.dragHandler_ = () => 0;
    this.chip_ = this.getRequiredElement('#chip');
    this.chip_.setAttribute('draggable', 'true');

    this.chip_.addEventListener(
        'dragend',
        (event: MouseEvent) =>
            this.dragHandler_(this, event.clientX, event.clientY));
  }

  set dragEndHandler(
      handler: (element: TabGroupElement, x: number, y: number) => void) {
    this.dragHandler_ = handler;
  }

  updateVisuals(visualData: TabGroupVisualData) {
    this.getRequiredElement('#title').innerText = visualData.title;
    this.style.setProperty('--tabstrip-tab-group-color-rgb', visualData.color);
    this.style.setProperty(
        '--tabstrip-tab-group-text-color-rgb', visualData.textColor);

    if (visualData.title) {
      this.chip_.setAttribute(
          'aria-label',
          loadTimeData.getStringF(
              'namedGroupLabel', '', visualData.title, '', ''));
    } else {
      this.chip_.setAttribute(
          'aria-label',
          loadTimeData.getStringF('unnamedGroupLabel', '', '', ''));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-tab-group-playground': TabGroupElement;
  }
}

customElements.define('tabstrip-tab-group-playground', TabGroupElement);

export function isTabGroupElement(element: Element): boolean {
  return element.tagName === 'TABSTRIP-TAB-GROUP-PLAYGROUND';
}
