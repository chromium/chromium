// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {isTabElement, TabElement} from './tab.js';
import {isTabGroupElement, TabGroupElement} from './tab_group.js';
import {TabStripEmbedderProxy, TabStripEmbedderProxyImpl} from './tab_strip_embedder_proxy.js';
import {TabData, TabNetworkState, TabsApiProxy, TabsApiProxyImpl} from './tabs_api_proxy.js';

/** @const {number} */
export const PLACEHOLDER_TAB_ID = -1;

/** @const {string} */
export const PLACEHOLDER_GROUP_ID = 'placeholder';

/**
 * The data type key for pinned state of a tab. Since drag events only expose
 * whether or not a data type exists (not the actual value), presence of this
 * data type means that the tab is pinned.
 * @const {string}
 */
const PINNED_DATA_TYPE = 'pinned';

/**
 * Gets the data type of tab IDs on DataTransfer objects in drag events. This
 * is a function so that loadTimeData can get overridden by tests.
 * @return {string}
 */
function getTabIdDataType() {
  return loadTimeData.getString('tabIdDataType');
}

/** @return {string} */
function getGroupIdDataType() {
  return loadTimeData.getString('tabGroupIdDataType');
}

/** @return {!TabData} */
function getDefaultTabData() {
  return {
    active: false,
    alertStates: [],
    blocked: false,
    crashed: false,
    id: -1,
    index: -1,
    isDefaultFavicon: false,
    networkState: TabNetworkState.NONE,
    pinned: false,
    shouldHideThrobber: false,
    showIcon: true,
    title: '',
    url: '',
  };
}

/**
 * @interface
 */
export class DragManagerDelegate {
  /**
   * @param {!TabElement} tabElement
   * @return {number}
   */
  getIndexOfTab(tabElement) {}

  /**
   * @param {!TabElement} element
   * @param {number} index
   * @param {boolean} pinned
   * @param {string=} groupId
   */
  placeTabElement(element, index, pinned, groupId) {}

  /**
   * @param {!TabGroupElement} element
   * @param {number} index
   */
  placeTabGroupElement(element, index) {}

  /** @return {boolean} */
  shouldPreventDrag() {}
}

/** @typedef {!DragManagerDelegate|!HTMLElement} */
let DragManagerDelegateElement;

class DragSession {
  /**
   * @param {!DragManagerDelegateElement} delegate
   * @param {!TabElement|!TabGroupElement} element
   * @param {number} srcIndex
   * @param {string=} srcGroup
   */
  constructor(delegate, element, srcIndex, srcGroup) {
    /** @const @private {!DragManagerDelegateElement} */
    this.delegate_ = delegate;

    /** @const {!TabElement|!TabGroupElement} */
    this.element_ = element;

    /**
     * Flag indicating if during the drag session, the element has at least
     * moved once.
     * @private {boolean}
     */
    this.hasMoved_ = false;

    /** @private {!Object<{x: number, y: number}>} */
    this.lastPoint_ = {x: 0, y: 0};

    /** @const {number} */
    this.srcIndex = srcIndex;

    /** @const {string|undefined} */
    this.srcGroup = srcGroup;

    /** @private @const {!TabsApiProxy} */
    this.tabsProxy_ = TabsApiProxyImpl.getInstance();

    /** @private {!TabStripEmbedderProxy} */
    this.tabStripEmbedderProxy_ = TabStripEmbedderProxyImpl.getInstance();
  }

  /**
   * @param {!DragManagerDelegateElement} delegate
   * @param {!TabElement|!TabGroupElement} element
   * @return {!DragSession}
   */
  static createFromElement(delegate, element) {
    if (isTabGroupElement(element)) {
      return new DragSession(
          delegate, element,
          delegate.getIndexOfTab(
              /** @type {!TabElement} */ (element.firstElementChild)));
    }

    const srcIndex = delegate.getIndexOfTab(
        /** @type {!TabElement} */ (element));
    const srcGroup =
        (element.parentElement && isTabGroupElement(element.parentElement)) ?
        element.parentElement.dataset.groupId :
        undefined;
    return new DragSession(delegate, element, srcIndex, srcGroup);
  }

  /**
   * @param {!DragManagerDelegateElement} delegate
   * @param {!DragEvent} event
   * @return {?DragSession}
   */
  static createFromEvent(delegate, event) {
    if (event.dataTransfer.types.includes(getTabIdDataType())) {
      const isPinned = event.dataTransfer.types.includes('pinned');
      const placeholderTabElement =
          /** @type {!TabElement} */ (document.createElement('tabstrip-tab'));
      placeholderTabElement.tab = /** @type {!TabData} */ (Object.assign(
          getDefaultTabData(), {id: PLACEHOLDER_TAB_ID, pinned: isPinned}));
      placeholderTabElement.setDragging(true);
      delegate.placeTabElement(placeholderTabElement, -1, isPinned);
      return DragSession.createFromElement(delegate, placeholderTabElement);
    }

    if (event.dataTransfer.types.includes(getGroupIdDataType())) {
      const placeholderGroupElement = /** @type {!TabGroupElement} */
          (document.createElement('tabstrip-tab-group'));
      placeholderGroupElement.dataset.groupId = PLACEHOLDER_GROUP_ID;
      placeholderGroupElement.setDragging(true);
      delegate.placeTabGroupElement(placeholderGroupElement, -1);
      return DragSession.createFromElement(delegate, placeholderGroupElement);
    }

    return null;
  }

  /** @return {string|undefined} */
  get dstGroup() {
    if (isTabElement(this.element_) && this.element_.parentElement &&
        isTabGroupElement(this.element_.parentElement)) {
      return this.element_.parentElement.dataset.groupId;
    }

    return undefined;
  }

  /** @return {number} */
  get dstIndex() {
    if (isTabElement(this.element_)) {
      return this.delegate_.getIndexOfTab(
          /** @type {!TabElement} */ (this.element_));
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
        return this.delegate_.getIndexOfTab(
                   /** @private {!TabElement} */ (previousElement)) +
            1;
      }

      assert(isTabGroupElement(previousElement));
      return this.delegate_.getIndexOfTab(/** @private {!TabElement} */ (
                 previousElement.lastElementChild)) +
          1;
    }

    // If a tab group is moving backwards (to the front of the tab strip), the
    // new index is the index of the first tab in that group. If a tab group is
    // moving forwards (to the end of the tab strip), the new index is the index
    // of the last tab in that group.
    let dstIndex = this.delegate_.getIndexOfTab(
        /** @type {!TabElement} */ (this.element_.firstElementChild));
    if (this.srcIndex <= dstIndex) {
      dstIndex += this.element_.childElementCount - 1;
    }
    return dstIndex;
  }

  /** @param {!DragEvent} event */
  cancel(event) {
    if (this.isDraggingPlaceholder()) {
      this.element_.remove();
      return;
    }

    if (isTabGroupElement(this.element_)) {
      this.delegate_.placeTabGroupElement(
          /** @type {!TabGroupElement} */ (this.element_), this.srcIndex);
    } else if (isTabElement(this.element_)) {
      this.delegate_.placeTabElement(
          /** @type {!TabElement} */ (this.element_), this.srcIndex,
          this.element_.tab.pinned, this.srcGroup);
    }

    if (this.element_.isDraggedOut() &&
        event.dataTransfer.dropEffect === 'move') {
      // The element was dragged out of the current tab strip and was dropped
      // into a new window. In this case, do not mark the element as no longer
      // being dragged out. The element needs to be kept hidden, and will be
      // automatically removed from the DOM with the next tab-removed event.
      return;
    }

    this.element_.setDragging(false);
    this.element_.setDraggedOut(false);
  }

  /** @return {boolean} */
  isDraggingPlaceholder() {
    return this.isDraggingPlaceholderTab_() ||
        this.isDraggingPlaceholderGroup_();
  }

  /**
   * @return {boolean}
   * @private
   */
  isDraggingPlaceholderTab_() {
    return isTabElement(this.element_) &&
        this.element_.tab.id === PLACEHOLDER_TAB_ID;
  }

  /**
   * @return {boolean}
   * @private
   */
  isDraggingPlaceholderGroup_() {
    return isTabGroupElement(this.element_) &&
        this.element_.dataset.groupId === PLACEHOLDER_GROUP_ID;
  }

  /** @param {!DragEvent} event */
  finish(event) {
    const wasDraggingPlaceholder = this.isDraggingPlaceholderTab_();
    if (wasDraggingPlaceholder) {
      const id = Number(event.dataTransfer.getData(getTabIdDataType()));
      this.element_.tab = Object.assign({}, this.element_.tab, {id});
    } else if (this.isDraggingPlaceholderGroup_()) {
      this.element_.dataset.groupId =
          event.dataTransfer.getData(getGroupIdDataType());
    }

    const dstIndex = this.dstIndex;
    if (isTabElement(this.element_)) {
      this.tabsProxy_.moveTab(this.element_.tab.id, dstIndex);
    } else if (isTabGroupElement(this.element_)) {
      this.tabsProxy_.moveGroup(this.element_.dataset.groupId, dstIndex);
    }

    const dstGroup = this.dstGroup;
    if (dstGroup && dstGroup !== this.srcGroup) {
      this.tabsProxy_.groupTab(this.element_.tab.id, dstGroup);
    } else if (!dstGroup && this.srcGroup) {
      this.tabsProxy_.ungroupTab(this.element_.tab.id);
    }

    this.element_.setDragging(false);
    this.element_.setDraggedOut(false);
  }

  /**
   * @param {!TabElement|!TabGroupElement} dragOverElement
   * @return {boolean}
   */
  shouldOffsetIndexForGroup_(dragOverElement) {
    // Since TabGroupElements do not have any TabElements, they need to offset
    // the index for any elements that come after it as if there is at least
    // one element inside of it.
    return this.isDraggingPlaceholder() &&
        !!(dragOverElement.compareDocumentPosition(this.element_) &
           Node.DOCUMENT_POSITION_PRECEDING);
  }

  /** @param {!DragEvent} event */
  start(event) {
    this.lastPoint_ = {x: event.clientX, y: event.clientY};
    event.dataTransfer.effectAllowed = 'move';
    const draggedItemRect = event.composedPath()[0].getBoundingClientRect();
    this.element_.setDragging(true);

    const dragImage = this.element_.getDragImage();
    const dragImageRect = dragImage.getBoundingClientRect();

    let scaleFactor = 1;
    let verticalOffset = 0;

    // <if expr="chromeos">
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

    event.dataTransfer.setDragImage(dragImage, xOffset, yOffset);

    if (isTabElement(this.element_)) {
      event.dataTransfer.setData(
          getTabIdDataType(), this.element_.tab.id.toString());

      if (this.element_.tab.pinned) {
        event.dataTransfer.setData(
            'pinned', this.element_.tab.pinned.toString());
      }
    } else if (isTabGroupElement(this.element_)) {
      event.dataTransfer.setData(
          getGroupIdDataType(), this.element_.dataset.groupId);
    }
  }

  /** @param {!DragEvent} event */
  update(event) {
    this.lastPoint_ = {x: event.clientX, y: event.clientY};

    if (event.type === 'dragleave') {
      this.element_.setDraggedOut(true);
      this.hasMoved_ = true;
      return;
    }

    event.dataTransfer.dropEffect = 'move';
    this.element_.setDraggedOut(false);
    if (isTabGroupElement(this.element_)) {
      this.updateForTabGroupElement_(event);
    } else if (isTabElement(this.element_)) {
      this.updateForTabElement_(event);
    }
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  updateForTabGroupElement_(event) {
    const tabGroupElement =
        /** @type {!TabGroupElement} */ (this.element_);
    const composedPath = /** @type {!Array<!Element>} */ (event.composedPath());
    if (composedPath.includes(assert(this.element_))) {
      // Dragging over itself or a child of itself.
      return;
    }

    const dragOverTabElement =
        /** @type {!TabElement|undefined} */ (composedPath.find(isTabElement));
    if (dragOverTabElement && !dragOverTabElement.tab.pinned &&
        dragOverTabElement.isValidDragOverTarget) {
      let dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
      dragOverIndex +=
          this.shouldOffsetIndexForGroup_(dragOverTabElement) ? 1 : 0;
      this.delegate_.placeTabGroupElement(tabGroupElement, dragOverIndex);
      this.hasMoved_ = true;
      return;
    }

    const dragOverGroupElement = /** @type {!TabGroupElement|undefined} */ (
        composedPath.find(isTabGroupElement));
    if (dragOverGroupElement && dragOverGroupElement.isValidDragOverTarget) {
      let dragOverIndex = this.delegate_.getIndexOfTab(
          /** @type {!TabElement} */ (dragOverGroupElement.firstElementChild));
      dragOverIndex +=
          this.shouldOffsetIndexForGroup_(dragOverGroupElement) ? 1 : 0;
      this.delegate_.placeTabGroupElement(tabGroupElement, dragOverIndex);
      this.hasMoved_ = true;
    }
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  updateForTabElement_(event) {
    const tabElement = /** @type {!TabElement} */ (this.element_);
    const composedPath = /** @type {!Array<!Element>} */ (event.composedPath());
    const dragOverTabElement =
        /** @type {?TabElement} */ (composedPath.find(isTabElement));
    if (dragOverTabElement &&
        (dragOverTabElement.tab.pinned !== tabElement.tab.pinned ||
         !dragOverTabElement.isValidDragOverTarget)) {
      // Can only drag between the same pinned states and valid TabElements.
      return;
    }

    const previousGroupId = (tabElement.parentElement &&
                             isTabGroupElement(tabElement.parentElement)) ?
        tabElement.parentElement.dataset.groupId :
        undefined;

    const dragOverTabGroup =
        /** @type {?TabGroupElement} */ (composedPath.find(isTabGroupElement));
    if (dragOverTabGroup &&
        dragOverTabGroup.dataset.groupId !== previousGroupId &&
        dragOverTabGroup.isValidDragOverTarget) {
      this.delegate_.placeTabElement(
          tabElement, this.dstIndex, false, dragOverTabGroup.dataset.groupId);
      this.hasMoved_ = true;
      return;
    }

    if (!dragOverTabGroup && previousGroupId) {
      this.delegate_.placeTabElement(
          tabElement, this.dstIndex, false, undefined);
      this.hasMoved_ = true;
      return;
    }

    if (!dragOverTabElement) {
      return;
    }

    const dragOverIndex = this.delegate_.getIndexOfTab(dragOverTabElement);
    this.delegate_.placeTabElement(
        tabElement, dragOverIndex, tabElement.tab.pinned, previousGroupId);
    this.hasMoved_ = true;
  }
}

export class DragManager {
  /** @param {!DragManagerDelegateElement} delegate */
  constructor(delegate) {
    /** @private {!DragManagerDelegateElement} */
    this.delegate_ = delegate;

    /** @type {?DragSession} */
    this.dragSession_ = null;

    /** @private {!TabsApiProxy} */
    this.tabsProxy_ = TabsApiProxyImpl.getInstance();

    /** @private {!TabStripEmbedderProxy} */
    this.tabStripEmbedderProxy_ = TabStripEmbedderProxyImpl.getInstance();
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  onDragLeave_(event) {
    if (this.dragSession_ && this.dragSession_.isDraggingPlaceholder()) {
      this.dragSession_.cancel(event);
      this.dragSession_ = null;
      return;
    }

    this.dragSession_.update(event);
  }

  /** @param {!DragEvent} event */
  onDragOver_(event) {
    event.preventDefault();
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.update(event);
  }

  /** @param {!DragEvent} event */
  onDragStart_(event) {
    const draggedItem =
        /** @type {!Array<!Element>} */ (event.composedPath()).find(item => {
          return isTabElement(item) || isTabGroupElement(item);
        });
    if (!draggedItem) {
      return;
    }

    // If we are dragging a tab element ensure its touch pressed state is reset
    // to avoid any associated css effects making it onto the drag image.
    if (isTabElement(draggedItem)) {
      /** @private {!TabElement} */ (draggedItem).setTouchPressed(false);
    }

    if (this.delegate_.shouldPreventDrag()) {
      event.preventDefault();
      return;
    }

    this.dragSession_ = DragSession.createFromElement(
        this.delegate_,
        /** @type {!TabElement|!TabGroupElement} */ (draggedItem));
    this.dragSession_.start(event);
  }

  /** @param {!DragEvent} event */
  onDragEnd_(event) {
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.cancel(event);
    this.dragSession_ = null;
  }

  /** @param {!DragEvent} event */
  onDragEnter_(event) {
    if (this.dragSession_) {
      // TODO(crbug.com/843556): Do not update the drag session on dragenter.
      // An incorrect event target on dragenter causes tabs to move around
      // erroneously.
      return;
    }

    this.dragSession_ = DragSession.createFromEvent(this.delegate_, event);
  }

  /**
   * @param {!DragEvent} event
   */
  onDrop_(event) {
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.finish(event);
    this.dragSession_ = null;
  }

  startObserving() {
    this.delegate_.addEventListener(
        'dragstart', e => this.onDragStart_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'dragend', e => this.onDragEnd_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'dragenter', e => this.onDragEnter_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'dragleave', e => this.onDragLeave_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'dragover', e => this.onDragOver_(/** @type {!DragEvent} */ (e)));
    this.delegate_.addEventListener(
        'drop', e => this.onDrop_(/** @type {!DragEvent} */ (e)));
  }
}
