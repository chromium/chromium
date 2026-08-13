// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import sheet from './expandable_json_viewer.css' with {type : 'css'};
import {getTemplate} from './expandable_json_viewer.html.js';

/**
 * A container that expands to show its contents when clicked
 */
export class ExpandableJsonViewerElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private boundOnExpand_: () => void = () => this.onExpand();

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  connectedCallback() {
    const toggleElement = this.getRequiredElement('#json-header');
    toggleElement.addEventListener('click', this.boundOnExpand_);
  }

  disconnectedCallback() {
    const toggleElement = this.getRequiredElement('#json-header');
    toggleElement.removeEventListener('click', this.boundOnExpand_);
  }

  configure(child: HTMLElement, title: string) {
    const jsonViewerContent =
        this.getRequiredElement<HTMLElement>('#json-content');
    jsonViewerContent.appendChild(child);
    const titleElement = this.getRequiredElement('#title');
    titleElement.textContent = title || 'JSON Content';
  }

  getTitleTextForTesting(): string {
    const titleElement = this.getRequiredElement('#title');
    return titleElement.textContent || '';
  }

  /** @private */
  onExpand() {
    const expanded = !this.hasAttribute('expanded');
    this.toggleAttribute('expanded', expanded);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'expandable-json-viewer': ExpandableJsonViewerElement;
  }
}

customElements.define('expandable-json-viewer', ExpandableJsonViewerElement);
