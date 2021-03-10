// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {TabStripEmbedderProxy, TabStripEmbedderProxyImpl} from './tab_strip_embedder_proxy.js';
import {TabGroupVisualData} from './tabs_api_proxy.js';

export class TabGroupElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private @const {!TabStripEmbedderProxy} */
    this.embedderApi_ = TabStripEmbedderProxyImpl.getInstance();

    /** @private @const {!HTMLElement} */
    this.chip_ = /** @type {!HTMLElement} */ (this.$('#chip'));
    this.chip_.addEventListener('click', () => this.onClickChip_());
    this.chip_.addEventListener(
        'keydown', e => this.onKeydownChip_(/** @type {!KeyboardEvent} */ (e)));

    /**
     * Flag indicating if this element can accept dragover events. This flag
     * is updated by TabListElement while animating.
     * @private {boolean}
     */
    this.isValidDragOverTarget_ = true;
  }

  /** @return {boolean} */
  get isValidDragOverTarget() {
    return !this.hasAttribute('dragging_') && this.isValidDragOverTarget_;
  }

  /** @param {boolean} isValid */
  set isValidDragOverTarget(isValid) {
    this.isValidDragOverTarget_ = isValid;
  }

  /** @return {!HTMLElement} */
  getDragImage() {
    return /** @type {!HTMLElement} */ (this.$('#dragImage'));
  }

  /** @return {!HTMLElement} */
  getDragImageCenter() {
    // Since the drag handle is #chip, the drag image should be centered
    // relatively to it.
    return /** @type {!HTMLElement} */ (this.$('#chip'));
  }

  /** @private */
  onClickChip_() {
    if (!this.dataset.groupId) {
      return;
    }

    const boundingBox = this.$('#chip').getBoundingClientRect();
    this.embedderApi_.showEditDialogForGroup(
        this.dataset.groupId, boundingBox.left, boundingBox.top,
        boundingBox.width, boundingBox.height);
  }

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydownChip_(event) {
    if (event.key === 'Enter' || event.key === ' ') {
      this.onClickChip_();
    }
  }

  /** @param {boolean} enabled */
  setDragging(enabled) {
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

  /** @param {boolean} isDraggedOut */
  setDraggedOut(isDraggedOut) {
    this.toggleAttribute('dragged-out_', isDraggedOut);
  }

  /** @return {boolean} */
  isDraggedOut() {
    return this.hasAttribute('dragged-out_');
  }

  /**
   * @param {!TabGroupVisualData} visualData
   */
  updateVisuals(visualData) {
    this.$('#title').innerText = visualData.title;
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

customElements.define('tabstrip-tab-group', TabGroupElement);

/**
 * @param {!Element} element
 * @return {boolean}
 */
export function isTabGroupElement(element) {
  return element.tagName === 'TABSTRIP-TAB-GROUP';
}
