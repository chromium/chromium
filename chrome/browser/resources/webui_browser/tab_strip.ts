// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Tab as TabData, TabGroupVisualData} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';

import {TabElement} from './tab_element.js';
import {getCss} from './tab_strip.css.js';
import {getHtml} from './tab_strip.html.js';
import type {TabStripController} from './tab_strip_controller.js';

export interface TabStrip {
  $: {
    tabstrip: HTMLElement,
  };
}

export class TabStrip extends CrLitElement {
  static get is() {
    return 'webui-browser-tab-strip';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabStripController: {type: Object},
    };
  }

  controller?: TabStripController;
  private activeTab_: TabElement|null = null;
  protected tabs_: TabElement[] = [];

  addTab(tab: TabData) {
    const tabElement = new TabElement(tab);
    this.tabs_ = [...this.tabs_, tabElement];
    // Need to manually activate first tab.
    // TODO(webium): The tab strip API should sent an activation event.
    if (this.tabs_.length === 1) {
      this.activateTab(tab.id);
    }
    this.requestUpdate();
  }

  activateTab(tabId: NodeId) {
    if (this.activeTab_) {
      if (this.activeTab_.tabId === tabId) {
        return;
      }
      this.activeTab_.active = false;
    }
    const activeTab = this.tabs_.find(tab => tab.tabId === tabId);
    assert(activeTab);
    this.activeTab_ = activeTab!;
    this.activeTab_.active = true;
  }

  setTabGroupForTab(/*tabId*/ _: NodeId, /*groupId*/ _2?: NodeId) {
    // TODO(webium): implement this.
  }

  setTabGroupVisualData(_: string, _2: TabGroupVisualData) {
    // TODO(webium): implement this.
  }

  updateTab(tabData: TabData) {
    const tab = this.tabs_.find(tab => tab.tabId === tabData.id);
    // This function is called before the tab is created.
    if (!tab) {
      return;
    }

    tab.updateData(tabData);
  }

  removeTab(tabId: NodeId) {
    this.tabs_ = this.tabs_.filter(tab => tab.tabId !== tabId);
    this.requestUpdate();
  }

  protected onAddTab_() {
    this.dispatchEvent(
        new CustomEvent('tab-add', {bubbles: true, composed: true}));
  }

  // Drag experience variables.
  private dragging = false;
  private mouseX = 0;
  // TranslateX is the visual x coordinate difference from the bounding client's
  // x value.
  private translateX = 0;
  // tabOrderX represents the client rect x value. This can be modified when
  // swapping bounding client x values.
  private tabOrderX = 0;
  // tabInitialX also represents the client rect x value but will not be
  // modified if swapping tab order. This is the value that will be compared
  // against translateX to find the visual x values.
  private tabInitialX = 0;

  // Out of bounds dragOffset.
  private outOfBoundsDragX = 0;
  private outOfBoundsDragY = 0;

  // Set during drag events.
  private tabElement: TabElement|null = null;

  dragMouseDown(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();
    const path = e.composedPath();
    const tabElement = path.find(
        el => el instanceof Element && el.localName === 'webui-browser-tab');
    if (tabElement) {
      this.tabElement = tabElement as TabElement;
      this.dragging = true;
      this.mouseX = e.clientX;
      this.tabOrderX = this.tabElement.getBoundingClientRect().left;
      this.tabInitialX = this.tabElement.getBoundingClientRect().left;
      this.outOfBoundsDragX = e.clientX;
      this.outOfBoundsDragY = e.clientY;
      this.$.tabstrip.classList.add('nodrag');
      this.activateTab(this.tabElement.tabId);
      this.requestUpdate();
    }
  }

  closeDragElement() {
    if (!this.dragging) {
      return;
    }

    this.dragging = false;
    this.tabElement = null;
    this.translateX = 0;
    this.mouseX = 0;
    this.tabOrderX = 0;
    this.tabInitialX = 0;
    this.$.tabstrip.classList.remove('nodrag');

    // Reset the transform back to 0.
    this.tabs_.forEach(tabElement => {
      tabElement.style.transform = 'translateX(0px)';
    });
    this.requestUpdate();
  }

  elementDrag(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();
    if (!this.dragging) {
      return;
    }

    assert(this.tabElement);

    this.translateX = this.translateX - (this.mouseX - e.clientX);
    this.mouseX = e.clientX;

    // Set the tab's new position.
    this.tabElement.style.transform = `translateX(${this.translateX}px)`;

    // Reorder the tab if the shift has been more than tab width.
    //
    // This represents the tab's x coordinate after translation.
    const dragElementVisualX = this.tabInitialX + this.translateX;
    const tabThreshold = this.tabElement.offsetWidth / 2;
    // If the x value exceeds the bounds of the tab element, shift it left or
    // right.
    if (dragElementVisualX > (this.tabOrderX + tabThreshold) ||
        dragElementVisualX < (this.tabOrderX - tabThreshold)) {
      const index = this.tabs_.indexOf(this.tabElement);
      const direction =
          (dragElementVisualX > (this.tabOrderX + tabThreshold)) ? 1 : -1;
      const swapIndex = index + direction;
      if (index !== -1 && index < this.tabs_.length &&
          swapIndex < this.tabs_.length && swapIndex !== -1 &&
          this.tabs_[index] && this.tabs_[swapIndex]) {
        this.tabOrderX = this.tabs_[swapIndex].getBoundingClientRect().left;
        // Move the swapped tab by N pixels in the other direction.
        const swappedTabTranslateX = this.tabs_[swapIndex].getTransformX() +
            this.tabElement.offsetWidth * (direction * -1);
        this.tabs_[swapIndex].style.transform =
            `translateX(${swappedTabTranslateX}px)`;
        [this.tabs_[index], this.tabs_[swapIndex]] =
            [this.tabs_[swapIndex], this.tabs_[index]];
      }
    }
    // Check if tab is being dragged outside of bounds +/- artificial margins.
    if (e.clientX < this.getBoundingClientRect().left ||
        e.clientX >= this.getBoundingClientRect().right - 1 ||
        e.clientY < this.getBoundingClientRect().top ||
        e.clientY > this.getBoundingClientRect().bottom + 10) {
      this.outOfBoundsHandler(this.tabElement.tabId);
    }
  }

  private outOfBoundsHandler(tabId: NodeId) {
    this.dispatchEvent(new CustomEvent('tab-drag-out-of-bounds', {
      bubbles: true,
      composed: true,
      detail: {
        tabId: tabId,
        drag_offset_x: this.outOfBoundsDragX,
        drag_offset_y: this.outOfBoundsDragY,
      },
    }));
    this.closeDragElement();
  }
}

customElements.define(TabStrip.is, TabStrip);
