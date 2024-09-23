// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {TabElement} from './tab.js';
import {isTabElement} from './tab.js';
import type {TabGroupElement} from './tab_group.js';
import {isDragHandle, isTabGroupElement} from './tab_group.js';
import type {Tab} from './tab_strip.mojom-webui.js';
import {TabNetworkState} from './tab_strip.mojom-webui.js';
import type {TabsApiProxy} from './tabs_api_proxy.js';
import {TabsApiProxyImpl} from './tabs_api_proxy.js';

export const PLACEHOLDER_TAB_ID: number = -1;

export const PLACEHOLDER_GROUP_ID: string = 'placeholder';

/**
 * The data type key for pinned state of a tab. Since drag events only expose
 * whether or not a data type exists (not the actual value), presence of this
 * data type means that the tab is pinned.
 */
const PINNED_DATA_TYPE: string = 'pinned';

/**
 * Gets the data type of tab IDs on DataTransfer objects in drag events. This
 * is a function so that loadTimeData can get overridden by tests.
 */
function getTabIdDataType(): string {
  return loadTimeData.getString('tabIdDataType');
}

function getGroupIdDataType(): string {
  return loadTimeData.getString('tabGroupIdDataType');
}

function getDefaultTabData(): Tab {
  return {
    active: false,
    alertStates: [],
    blocked: false,
    crashed: false,
    id: -1,
    index: -1,
    isDefaultFavicon: false,
    networkState: TabNetworkState.kNone,
    pinned: false,
    shouldHideThrobber: false,
    showIcon: true,
    title: '',
    url: {url: ''},
    faviconUrl: null,
    activeFaviconUrl: null,
    groupId: null,
  };
}

export interface DragManagerDelegate {
  getIndexOfTab(tabElement: TabElement): number;

  placeTabElement(
      element: TabElement, index: number, pinned: boolean,
      groupId: string|null): void;

  placeTabGroupElement(element: TabGroupElement, index: number): void;

  shouldPreventDrag(isDraggingTab: boolean): boolean;
}

type DragManagerDelegateElement = DragManagerDelegate&HTMLElement;

class DragSession {
  private delegate_: DragManagerDelegateElement;
  private element_: TabElement|TabGroupElement;

  srcIndex: number;
  srcGroup: string|null;

  private tabsProxy_: TabsApiProxy = TabsApiProxyImpl.getInstance();

  constructor(
      delegate: DragManagerDelegateElement, element: TabElement|TabGroupElement,
      srcIndex: number, srcGroup: string|null) {
    this.delegate_ = delegate;
    this.element_ = element;

    this.srcIndex = srcIndex;
    this.srcGroup = srcGroup;
  }

  static createFromElement(
      delegate: DragManagerDelegateElement,
      element: TabElement|TabGroupElement): DragSession {
    if (isTabGroupElement(element)) {
      return new DragSession(
          delegate, element,
          delegate.getIndexOfTab(element.firstElementChild as TabElement),
          null);
    }

    const srcIndex = delegate.getIndexOfTab(element as TabElement);
    const srcGroup =
        (element.parentElement && isTabGroupElement(element.parentElement)) ?
        element.parentElement.dataset['groupId']! :
        null;
    return new DragSession(delegate, element, srcIndex, srcGroup);
  }

  static createFromEvent(
      delegate: DragManagerDelegateElement, event: DragEvent): DragSession
      |null {
    if (event.dataTransfer!.types.includes(getTabIdDataType())) {
      const isPinned = event.dataTransfer!.types.includes(PINNED_DATA_TYPE);
      const placeholderTabElement = document.createElement('tabstrip-tab');
      placeholderTabElement.tab = Object.assign(
          getDefaultTabData(), {id: PLACEHOLDER_TAB_ID, pinned: isPinned});
      placeholderTabElement.setDragging(true);
      delegate.placeTabElement(placeholderTabElement, -1, isPinned, null);
      return DragSession.createFromElement(delegate, placeholderTabElement);
    }

    if (event.dataTransfer!.types.includes(getGroupIdDataType())) {
      const placeholderGroupElement =
          document.createElement('tabstrip-tab-group');
      placeholderGroupElement.dataset['groupId'] = PLACEHOLDER_GROUP_ID;
      placeholderGroupElement.setDragging(true);
      delegate.placeTabGroupElement(placeholderGroupElement, -1);
      return DragSession.createFromElement(delegate, placeholderGroupElement);
    }

    return null;
  }

  get dstGroup(): string|undefined {
    if (isTabElement(this.element_) && this.element_.parentElement &&
        isTabGroupElement(this.element_.parentElement)) {
      return this.element_.parentElement.dataset['groupId'];
    }

    return undefined;
  }

  get dstIndex(): number {
    if (isTabElement(this.element_)) {
      return this.delegate_.getIndexOfTab(this.element_ as TabElement);
    }

    if (this.element_.children.length === 0) {
      // If this group element has no children, it was a placeholder element
      // being dragged. Find out the destination index by finding the index of
      // the tab closest to it and incrementing it by 1.
      const previousElement = this.element_.previousElementSibling;
      if (!previousElement) {
        return 0;
      }
      if (isTabElement(previousElement)) {
        return this.delegate_.getIndexOfTab(previousElement as TabElement) + 1;
      }

      assert(isTabGroupElement(previousElement));
      return this.delegate_.getIndexOfTab(
                 previousElement.lastElementChild as TabElement) +
          1;
    }

    const dstIndex = this.delegate_.getIndexOfTab(
        this.element_.firstElementChild as TabElement);

    return dstIndex;
  }

  cancel(event: DragEvent) {
    if (this.isDraggingPlaceholder()) {
      this.element_.remove();
      return;
    }

    if (isTabGroupElement(this.element_)) {
      this.delegate_.placeTabGroupElement(
          this.element_ as TabGroupElement, this.srcIndex);
    } else if (isTabElement(this.element_)) {
      const tabElement = this.element_ as TabElement;
      this.delegate_.placeTabElement(
          tabElement, this.srcIndex, tabElement.tab.pinned, this.srcGroup);
    }

    if (this.element_.isDraggedOut() &&
        event.dataTransfer!.dropEffect === 'move') {
      // The element was dragged out of the current tab strip and was dropped
      // into a new window. In this case, do not mark the element as no longer
      // being dragged out. The element needs to be kept hidden, and will be
      // automatically removed from the DOM with the next tab-removed event.
      return;
    }

    this.element_.setDragging(false);
    this.element_.setDraggedOut(false);
  }

  isDraggingPlaceholder(): boolean {
    return this.isDraggingPlaceholderTab_() ||
        this.isDraggingPlaceholderGroup_();
  }

  private isDraggingPlaceholderTab_(): boolean {
    return isTabElement(this.element_) &&
        (this.element_ as TabElement).tab.id === PLACEHOLDER_TAB_ID;
  }

  private isDraggingPlaceholderGroup_(): boolean {
    return isTabGroupElement(this.element_) &&
        this.element_.dataset['groupId'] === PLACEHOLDER_GROUP_ID;
  }

  finish(event: DragEvent) {
    const wasDraggingPlaceholder = this.isDraggingPlaceholderTab_();
    if (wasDraggingPlaceholder) {
      const id = Number(event.dataTransfer!.getData(getTabIdDataType()));
      (this.element_ as TabElement).tab =
          Object.assign({}, (this.element_ as TabElement).tab, {id});
    } else if (this.isDraggingPlaceholderGroup_()) {
      this.element_.dataset['groupId'] =
          event.dataTransfer!.getData(getGroupIdDataType());
    }

    const dstIndex = this.dstIndex;
    if (isTabElement(this.element_)) {
      this.tabsProxy_.moveTab((this.element_ as TabElement).tab.id, dstIndex);
    } else if (isTabGroupElement(this.element_)) {
      this.tabsProxy_.moveGroup(this.element_.dataset['groupId']!, dstIndex);
    }

    const dstGroup = this.dstGroup;
    if (dstGroup && dstGroup !== this.srcGroup) {
      this.tabsProxy_.groupTab((this.element_ as TabElement).tab.id, dstGroup);
    } else if (!dstGroup && this.srcGroup) {
      this.tabsProxy_.ungroupTab((this.element_ as TabElement).tab.id);
    }

    this.element_.setDragging(false);
    this.element_.setDraggedOut(false);
  }

  private shouldOffsetIndexForGroup_(dragOverElement: TabElement|
                                     TabGroupElement): boolean {
    // Since TabGroupElements do not have any TabElements, they need to offset
    // the index for any elements that come after it as if there is at least
    // one element inside of it.
    return this.isDraggingPlaceholder() &&
        !!(dragOverElement.compareDocumentPosition(this.element_) &
           Node.DOCUMENT_POSITION_PRECEDING);
  }

  start(event: DragEvent) {
    event.dataTransfer!.effectAllowed = 'move';
    const draggedItemRect =
        (event.composedPath()[0] as HTMLElement).getBoundingClientRect();
    this.element_.setDragging(true);

    const dragImage = this.element_.getDragImage();
    const dragImageRect = dragImage.getBoundingClientRect();

    let scaleFactor = 1;
    let verticalOffset = 0;

    // <if expr="chromeos_ash">
    // Touch on ChromeOS automatically scales drag images by 1.2 and adds a
    // vertical offset of 25px. See //ash/drag_drop/drag_drop_controller.cc.
    scaleFactor = 1.2;
    verticalOffset = 25;
    // </if>

    const eventXPercentage =
        (event.clientX - draggedItemRect.left) / draggedItemRect.width;
    const eventYPercentage =
        (event.clientY - draggedItemRect.top) / draggedItemRect.height;

    // First, align the top-left corner of the drag image's center element
    // to the event's coordinates.
    const dragImageCenterRect =
        this.element_.getDragImageCenter().getBoundingClientRect();
    let xOffset = (dragImageCenterRect.left - dragImageRect.left) * scaleFactor;
    let yOffset = (dragImageCenterRect.top - dragImageRect.top) * scaleFactor;

    // Then, offset the drag image again by using the event's coordinates
    // within the dragged item itself so that the drag image appears positioned
    // as closely as its state before dragging.
    xOffset += dragImageCenterRect.width * eventXPercentage;
    yOffset += dragImageCenterRect.height * eventYPercentage;
    yOffset -= verticalOffset;

    event.dataTransfer!.setDragImage(dragImage, xOffset, yOffset);

    if (isTabElement(this.element_)) {
      const tabElement = this.element_ as TabElement;
      event.dataTransfer!.setData(
          getTabIdDataType(), tabElement.tab.id.toString());

      if (tabElement.tab.pinned) {
        event.dataTransfer!.setData(
            PINNED_DATA_TYPE, tabElement.tab.pinned.toString());
      }
    } else if (isTabGroupElement(this.element_)) {
      event.dataTransfer!.setData(
          getGroupIdDataType(), this.element_.dataset['groupId']!);
    }
  }

  update(event: DragEvent) {
    if (event.type === 'dragleave') {
      this.element_.setDraggedOut(true);
      return;
    }

    event.dataTransfer!.dropEffect = 'move';
    this.element_.setDraggedOut(false);
    if (isTabGroupElement(this.element_)) {
      this.updateForTabGroupElement_(event);
    } else if (isTabElement(this.element_)) {
      this.updateForTabElement_(event);
    }
  }

  private updateForTabGroupElement_(event: DragEvent) {
    const tabGroupElement = this.element_ as TabGroupElement;
    const composedPath = event.composedPath() as Element[];
    if (composedPath.includes(this.element_)) {
      // Dragging over itself or a child of itself.
      return;
    }

    const dragOverTabElement =
        composedPath.find(isTabElement) as TabElement | undefined;
    if (dragOverTabElement && !dragOverTabElement.tab.pinned &&
        dragOverTabElement.isValidDragOverTarget) {
      let dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
      dragOverIndex +=
          this.shouldOffsetIndexForGroup_(dragOverTabElement) ? 1 : 0;
      this.delegate_.placeTabGroupElement(tabGroupElement, dragOverIndex);
      return;
    }

    const dragOverGroupElement =
        composedPath.find(isTabGroupElement) as TabGroupElement | undefined;
    if (dragOverGroupElement && dragOverGroupElement.isValidDragOverTarget) {
      let dragOverIndex = this.delegate_.getIndexOfTab(
          dragOverGroupElement.firstElementChild as TabElement);
      dragOverIndex +=
          this.shouldOffsetIndexForGroup_(dragOverGroupElement) ? 1 : 0;
      this.delegate_.placeTabGroupElement(tabGroupElement, dragOverIndex);
    }
  }

  private updateForTabElement_(event: DragEvent) {
    const tabElement = this.element_ as TabElement;
    const composedPath = event.composedPath() as Element[];
    const dragOverTabElement =
        composedPath.find(isTabElement) as TabElement | undefined;
    if (dragOverTabElement &&
        (dragOverTabElement.tab.pinned !== tabElement.tab.pinned ||
         !dragOverTabElement.isValidDragOverTarget)) {
      // Can only drag between the same pinned states and valid TabElements.
      return;
    }

    const previousGroupId = (tabElement.parentElement &&
                             isTabGroupElement(tabElement.parentElement)) ?
        tabElement.parentElement.dataset['groupId']! :
        null;

    const dragOverTabGroup =
        composedPath.find(isTabGroupElement) as TabGroupElement | undefined;
    if (dragOverTabGroup &&
        dragOverTabGroup.dataset['groupId'] !== previousGroupId &&
        dragOverTabGroup.isValidDragOverTarget) {
      this.delegate_.placeTabElement(
          tabElement, this.dstIndex, false,
          dragOverTabGroup.dataset['groupId'] || null);
      return;
    }

    if (!dragOverTabGroup && previousGroupId) {
      this.delegate_.placeTabElement(tabElement, this.dstIndex, false, null);
      return;
    }

    if (!dragOverTabElement) {
      return;
    }

    const dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
    this.delegate_.placeTabElement(
        tabElement, dragOverIndex, tabElement.tab.pinned, previousGroupId);
  }
}

export class DragManager {
  private delegate_: DragManagerDelegateElement;
  private dragSession_: DragSession|null = null;

  constructor(delegate: DragManagerDelegateElement) {
    this.delegate_ = delegate;
  }

  private onDragLeave_(event: DragEvent) {
    if (this.dragSession_ && this.dragSession_.isDraggingPlaceholder()) {
      this.dragSession_.cancel(event);
      this.dragSession_ = null;
      return;
    }

    this.dragSession_!.update(event);
  }

  private onDragOver_(event: DragEvent) {
    event.preventDefault();
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.update(event);
  }

  private onDragStart_(event: DragEvent) {
    const composedPath = event.composedPath() as Element[];
    const draggedItem = composedPath.find(item => {
      return isTabElement(item) || isTabGroupElement(item);
    });
    if (!draggedItem) {
      return;
    }

    // If we are dragging a tab or tab group element ensure its touch pressed
    // state is reset to avoid any associated css effects making it onto the
    // drag image.
    if (isTabElement(draggedItem) || isTabGroupElement(draggedItem)) {
      (draggedItem as TabElement | TabGroupElement).setTouchPressed(false);
    }

    // Make sure drag handle is under touch point when dragging a tab group.
    if (isTabGroupElement(draggedItem) && !composedPath.find(isDragHandle)) {
      return;
    }

    if (this.delegate_.shouldPreventDrag(isTabElement(draggedItem))) {
      event.preventDefault();
      return;
    }

    this.dragSession_ = DragSession.createFromElement(
        this.delegate_, draggedItem as TabElement | TabGroupElement);
    this.dragSession_.start(event);
  }

  private onDragEnd_(event: DragEvent) {
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.cancel(event);
    this.dragSession_ = null;
  }

  private onDragEnter_(event: DragEvent) {
    if (this.dragSession_) {
      // TODO(crbug.com/41389308): Do not update the drag session on dragenter.
      // An incorrect event target on dragenter causes tabs to move around
      // erroneously.
      return;
    }

    this.dragSession_ = DragSession.createFromEvent(this.delegate_, event);
  }

  private onDrop_(event: DragEvent) {
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.finish(event);
    this.dragSession_ = null;
  }

  startObserving() {
    this.delegate_.addEventListener('dragstart', e => this.onDragStart_(e));
    this.delegate_.addEventListener('dragend', e => this.onDragEnd_(e));
    this.delegate_.addEventListener('dragenter', e => this.onDragEnter_(e));
    this.delegate_.addEventListener('dragleave', e => this.onDragLeave_(e));
    this.delegate_.addEventListener('dragover', e => this.onDragOver_(e));
    this.delegate_.addEventListener('drop', e => this.onDrop_(e));
  }
}
