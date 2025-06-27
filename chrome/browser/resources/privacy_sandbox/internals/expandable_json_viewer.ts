// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './expandable_json_viewer.html.js';

/**
 * A container that expands to show its contents when clicked
 *
 * TODO(crbug.com/427546974): Add button to copy component contents.
 * TODO(crbug.com/427550387): Create a dedicated icon component and use it here.
 */
export class ExpandableJsonViewerElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  connectedCallback() {
    const toggleElement = this.shadowRoot!.querySelector('#json-header')!;
    toggleElement.addEventListener('click', () => this.onExpand());
  }

  configure(child: HTMLElement, title: string) {
    const jsonViewerContent =
        this.shadowRoot!.querySelector<HTMLElement>('#json-content')!;
    jsonViewerContent.appendChild(child);
    const titleElement = this.shadowRoot!.querySelector('#title')!;
    titleElement.textContent = title || 'JSON Content';
  }

  getTitleTextForTesting() {
    const titleElement = this.shadowRoot!.querySelector('#title');
    return titleElement ? titleElement.textContent : '';
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
