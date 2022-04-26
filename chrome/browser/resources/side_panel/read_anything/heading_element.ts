// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentNode} from './read_anything.mojom-webui.js';

export class HeadingElement extends PolymerElement {
  static get is() {
    return 'read-anything-heading';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      contentNode: Object,
    };
  }

  contentNode: ContentNode;

  /////////////////////////////////////
  // Called by heading_element.html. //
  /////////////////////////////////////

  private isH1_(): boolean {
    return this.contentNode.headingLevel === 1;
  }

  private isH2_(): boolean {
    return this.contentNode.headingLevel === 2;
  }

  private isH3_(): boolean {
    return this.contentNode.headingLevel === 3;
  }

  private isH4_(): boolean {
    return this.contentNode.headingLevel === 4;
  }

  private isH5_(): boolean {
    return this.contentNode.headingLevel === 5;
  }

  private isH6_(): boolean {
    return this.contentNode.headingLevel === 6;
  }

  private isUnknown_(): boolean {
    // If heading level was not specified or it is an unexpected value, return
    // true. This will force heading to be shown with the default heading level,
    // which is 2 according to ARIA 1.1.
    return !this.contentNode.headingLevel ||
        this.contentNode.headingLevel < 1 || this.contentNode.headingLevel > 6;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-heading': HeadingElement;
  }
}

customElements.define(HeadingElement.is, HeadingElement);
