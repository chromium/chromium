// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './tab_group.html.js';
import type {TabGroupVisualData} from './tab_strip.mojom-webui.js';
import type {TabsApiProxy} from './tabs_api_proxy.js';
import {TabsApiProxyImpl} from './tabs_api_proxy.js';

export class TabGroupElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private tabsApi_: TabsApiProxy;
  private chip_: HTMLElement;
  private isValidDragOverTarget_: boolean;

  constructor() {
    super();

    this.tabsApi_ = TabsApiProxyImpl.getInstance();

    this.chip_ = this.getRequiredElement('#chip');
    this.chip_.addEventListener('click', () => this.onClickChip_());
    this.chip_.addEventListener(
        'keydown', e => this.onKeydownChip_(/** @type {!KeyboardEvent} */ (e)));

    /**
     * Flag indicating if this element can accept dragover events. This flag
     * is updated by TabListElement while animating.
     */
    this.isValidDragOverTarget_ = true;
  }

  get isValidDragOverTarget(): boolean {
    return !this.hasAttribute('dragging_') && this.isValidDragOverTarget_;
  }

  set isValidDragOverTarget(isValid: boolean) {
    this.isValidDragOverTarget_ = isValid;
  }

  getDragImage(): HTMLElement {
    return this.getRequiredElement('#dragImage');
  }

  getDragImageCenter(): HTMLElement {
    // Since the drag handle is #dragHandle, the drag image should be
    // centered relatively to it.
    return this.getRequiredElement('#dragHandle');
  }

  private onClickChip_() {
    if (!this.dataset['groupId']) {
      return;
    }

    const boundingBox =
        this.getRequiredElement('#chip').getBoundingClientRect();
    this.tabsApi_.showEditDialogForGroup(
        this.dataset['groupId'], boundingBox.left, boundingBox.top,
        boundingBox.width, boundingBox.height);
  }

  private onKeydownChip_(event: KeyboardEvent) {
    if (event.key === 'Enter' || event.key === ' ') {
      this.onClickChip_();
    }
  }

  setDragging(enabled: boolean) {
    // Since the draggable target is the #chip, if the #chip moves and is no
    // longer under the pointer while the dragstart event is happening, the drag
    // will get canceled. This is unfortunately the behavior of the native drag
    // and drop API. The workaround is to have two different attributes: one
    // to get the drag image and start the drag event while keeping #chip in
    // place, and another to update the placeholder to take the place of where
    // the #chip would be.
    this.toggleAttribute('getting-drag-image_', enabled);
    requestAnimationFrame(() => {
      this.toggleAttribute('dragging', enabled);
    });
  }

  setDraggedOut(isDraggedOut: boolean) {
    this.toggleAttribute('dragged-out_', isDraggedOut);
  }

  isDraggedOut(): boolean {
    return this.hasAttribute('dragged-out_');
  }

  setTouchPressed(isTouchPressed: boolean) {
    this.toggleAttribute('touch_pressed_', isTouchPressed);
  }

  updateVisuals(visualData: TabGroupVisualData) {
    this.getRequiredElement('#title').innerText = visualData.title;
    this.style.setProperty('--tabstrip-tab-group-color-rgb', visualData.color);
    this.style.setProperty(
        '--tabstrip-tab-group-text-color-rgb', visualData.textColor);

    // Content strings are empty for the label and are instead replaced by
    // the aria-describedby attribute on the chip.
    if (visualData.title) {
      this.chip_.setAttribute(
          'aria-label',
          loadTimeData.getStringF('namedGroupLabel', visualData.title, ''));
    } else {
      this.chip_.setAttribute(
          'aria-label', loadTimeData.getStringF('unnamedGroupLabel', ''));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-tab-group': TabGroupElement;
  }
}

customElements.define('tabstrip-tab-group', TabGroupElement);

export function isTabGroupElement(element: Element): boolean {
  return element.tagName === 'TABSTRIP-TAB-GROUP';
}

export function isDragHandle(element: Element): boolean {
  return element.id === 'dragHandle';
}
