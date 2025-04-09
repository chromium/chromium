// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

/**
 * Navigates focus through nested elements given a HTMLElement parent
 * node with child nodes with specific element types such as `div` or custom
 * elements like `power-bookmark-row`
 *
 * This service can focus elements across shadow doms.
 */
export class KeyArrowNavigationService {
  /**
   * List of focusable HTMLElement nodes representing focusable elements in a
   * flat format sorted by tab order.
   */
  private elements_: HTMLElement[] = [];
  private rootElement_!: HTMLElement;
  private focusIndex_: number = 0;
  private childrenQuerySelector_: string = '';

  constructor(rootElement: HTMLElement, querySelector: string) {
    this.rootElement_ = rootElement;
    this.childrenQuerySelector_ = querySelector;
  }

  private boundKeyArrowListener_ = this.handleKeyArrowEvent_.bind(this);

  /**
   * Creates listeners for the root element.
   * Invoke during setup.
   */
  startListening() {
    this.rootElement_.addEventListener('keydown', this.boundKeyArrowListener_);
  }

  /**
   * Cleans up any listeners created by the startListening method.
   * Invoke during teardown.
   */
  stopListening() {
    this.rootElement_.removeEventListener(
        'keydown', this.boundKeyArrowListener_);
  }

  /**
   * Inserts elements to the `elements_` list next to the current `focusIndex_`
   * position.
   *
   * @param parentElement parent node from which nested elements will be added
   */
  addElementsWithin(parentElement: HTMLElement) {
    const childElements = this.traverseElements_(parentElement);

    const newElements = [...this.elements_];
    const targetIndex = this.findElementIndex_(parentElement);

    // adding child elements to the right of the parent element
    newElements.splice(targetIndex + 1, 0, ...childElements);

    this.elements_ = newElements;
  }

  /**
   * Collapses nested elements from a given HTMLElement node by traversing the
   * given element to verify if any child elements have nested children and
   * account for these when removing elements from the main elements list.
   *
   * @param parentElement parent node from which nested elements will be removed
   */
  removeElementsWithin(parentElement: HTMLElement) {
    const updatedElements = [...this.elements_];
    const numElementsToRemove = this.traverseElements_(parentElement).length;
    const targetIndex = this.findElementIndex_(parentElement);

    updatedElements.splice(targetIndex + 1, numElementsToRemove);
    this.elements_ = updatedElements;
  }

  /**
   * Used to manually focus on a specific element when the keyboard focus is
   * currently on an element, and a click event focuses on a different element
   * therefore having to move the `focusIndex` to the element being clicked.
   *
   * Returns false if no element was found.
   *
   * @param element Target element to focus on
   */
  setCurrentFocusIndex(element: HTMLElement): boolean {
    const newCurrentIndex = this.findElementIndex_(element);

    if (newCurrentIndex < 0) {
      return false;
    }

    this.focusIndex_ = newCurrentIndex;
    return true;
  }

  /**
   * Rebuilds the navigation structure from an optional given element, if not
   * it will default to the existing root node.
   *
   * @param rootElement
   */
  rebuildNavigationElements(rootElement?: HTMLElement) {
    this.elements_ = this.traverseElements_(rootElement || this.rootElement_);
  }

  /**
   * Returns the current focused element, may be used for testing.
   *
   * @returns the current focused element
   */
  getElementAtFocusIndexForTesting(): HTMLElement {
    return this.elements_[this.focusIndex_];
  }

  /**
   * Returns the focusable elements list, may be used for testing.
   *
   * @returns HTML element list
   */
  getElementsForTesting(): HTMLElement[] {
    return [...this.elements_];
  }

  private handleKeyArrowEvent_(event: KeyboardEvent) {
    const {key} = event;

    if (!(key === 'ArrowUp' || key === 'ArrowDown')) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();

    if (key === 'ArrowUp') {
      this.moveFocus_(-1);
    }
    if (key === 'ArrowDown') {
      this.moveFocus_(1);
    }
  }

  private moveFocus_(direction: -1|1) {
    if (this.focusIndex_ + direction > this.elements_.length - 1) {
      this.focusIndex_ = 0;
      this.focusCurrentIndex_();
      return;
    }
    if (this.focusIndex_ + direction < 0) {
      this.focusIndex_ = this.elements_.length - 1;
      this.focusCurrentIndex_();
      return;
    }

    this.focusIndex_ += direction;
    this.focusCurrentIndex_();
  }

  private focusCurrentIndex_() {
    this.elements_[this.focusIndex_].focus();
  }

  private findElementIndex_(element: HTMLElement): number {
    return this.elements_.findIndex((elem) => elem === element);
  }

  private traverseElements_(node: HTMLElement): HTMLElement[] {
    const children = Array.from(node.shadowRoot!.querySelectorAll<HTMLElement>(
        this.childrenQuerySelector_));
    let treeElements: HTMLElement[] = [];

    for (const childNode of children) {
      const hasChildren = childNode.shadowRoot &&
          Array.from(childNode.shadowRoot.querySelectorAll<HTMLElement>(
                         this.childrenQuerySelector_))
                  .length > 0;

      treeElements = hasChildren ?
          [
            ...treeElements,
            childNode,
            ...this.traverseElements_(childNode),
          ] :
          [...treeElements, childNode];
    }

    return treeElements;
  }

  static getInstance(): KeyArrowNavigationService {
    assert(instance);
    return instance;
  }

  static setInstance(obj: KeyArrowNavigationService) {
    instance = obj;
  }
}

let instance: KeyArrowNavigationService|null = null;
