// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

/**
 * Navigates focus through focusable elements using ArrowUp and ArrowDown keys.
 */
export class KeyArrowNavigationService {
  private elements_: HTMLElement[] = [];
  private rootElement_: HTMLElement;
  private focusIndex_: number = 0;
  private getNavigableElements_: () => HTMLElement[];

  private boundKeyArrowListener_ = this.handleKeyArrowEvent_.bind(this);
  private boundFocusInListener_ = this.handleFocusInEvent_.bind(this);

  constructor(
      rootElement: HTMLElement, getNavigableElements: () => HTMLElement[]) {
    this.rootElement_ = rootElement;
    this.getNavigableElements_ = getNavigableElements;
  }

  /**
   * Creates listeners for the root element.
   */
  startListening() {
    this.rootElement_.addEventListener('keydown', this.boundKeyArrowListener_);
    this.rootElement_.addEventListener('focusin', this.boundFocusInListener_);
  }

  /**
   * Cleans up listeners created by startListening.
   */
  stopListening() {
    this.rootElement_.removeEventListener(
        'keydown', this.boundKeyArrowListener_);
    this.rootElement_.removeEventListener(
        'focusin', this.boundFocusInListener_);
  }

  /**
   * Sets the current focus index to the specified element.
   * Returns false if the element was not found.
   */
  setCurrentFocusIndex(element: HTMLElement): boolean {
    this.elements_ = this.getNavigableElements_();
    const newCurrentIndex = this.findElementIndex_(element);
    if (newCurrentIndex < 0) {
      return false;
    }
    this.focusIndex_ = newCurrentIndex;
    return true;
  }

  /**
   * Rebuilds the navigation elements list.
   */
  rebuildNavigationElements() {
    this.elements_ = this.getNavigableElements_();
    if (this.elements_.length === 0) {
      this.focusIndex_ = 0;
      return;
    }
    this.focusIndex_ =
        Math.max(0, Math.min(this.focusIndex_, this.elements_.length - 1));
  }

  /**
   * Returns the element at current focus index for testing.
   */
  getElementAtFocusIndexForTesting(): HTMLElement {
    return this.elements_[this.focusIndex_]!;
  }

  /**
   * Returns the list of navigable elements for testing.
   */
  getElementsForTesting(): HTMLElement[] {
    return [...this.elements_];
  }

  /**
   * Returns the count of navigable elements.
   */
  getElementCount(): number {
    return this.elements_.length;
  }

  /**
   * Moves focus by direction (-1 for previous, 1 for next).
   */
  moveFocus(direction: -1|1) {
    this.elements_ = this.getNavigableElements_();
    const elementCount = this.elements_.length;
    if (elementCount === 0) {
      return;
    }

    if (direction === 1) {
      this.focusIndex_ = (this.focusIndex_ + 1) % elementCount;
    } else {
      this.focusIndex_ = (this.focusIndex_ - 1 + elementCount) % elementCount;
    }

    this.focusCurrentIndex_();
  }

  private handleFocusInEvent_(event: FocusEvent) {
    this.elements_ = this.getNavigableElements_();
    const path = event.composedPath();
    const target = this.elements_.find(elem => path.includes(elem));
    if (target) {
      this.focusIndex_ = this.elements_.indexOf(target);
    }
  }

  private handleKeyArrowEvent_(event: KeyboardEvent) {
    const {key} = event;
    if (key !== 'ArrowUp' && key !== 'ArrowDown') {
      return;
    }

    this.elements_ = this.getNavigableElements_();
    if (this.elements_.length === 0) {
      return;
    }

    const path = event.composedPath();
    const activeNavigableElement = this.elements_.find(el => path.includes(el));
    if (!activeNavigableElement) {
      return;
    }

    // Do not navigate on arrow keys if focus is within an open dialog (e.g.
    // an opened action menu).
    if (path.some(
            el => (el as HTMLElement).tagName === 'DIALOG' ||
                (el as HTMLElement).tagName === 'CR-ACTION-MENU')) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();

    this.focusIndex_ = this.elements_.indexOf(activeNavigableElement);

    if (key === 'ArrowUp') {
      this.moveFocus(-1);
    } else if (key === 'ArrowDown') {
      this.moveFocus(1);
    }
  }

  private focusCurrentIndex_() {
    const element = this.elements_[this.focusIndex_];
    if (element) {
      element.focus();
    }
  }

  private findElementIndex_(element: HTMLElement): number {
    return this.elements_.findIndex((elem) => elem === element);
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
