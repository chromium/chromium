// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './key_value_pair_entry.html.js';

export const COLLAPSE_THRESHOLD = 200;

export interface KeyValuePairEntry {
  key: string;
  value: string;
}

// <if expr="chromeos_ash">
// Link to markdown doc with documentation for Chrome OS.
const CROS_MD_DOC_URL =
    'https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/debugd/docs/log_entries.md';
// </if>

export class KeyValuePairEntryElement extends PolymerElement {
  static get is() {
    return 'key-value-pair-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      entry: Object,

      collapsible_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'computeCollabsible_(entry.statValue)',
      },

      collapsed: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },
    };
  }

  entry: KeyValuePairEntry;
  collapsed: boolean;
  private collapsible_: boolean;

  private getHref_(): string {
    // Let URL be anchor to the section of this page by default.
    let urlPrefix = '';

    // <if expr="chromeos_ash">
    // Link to the markdown doc with documentation for the entry for Chrome OS
    // instead.
    urlPrefix = CROS_MD_DOC_URL;
    // </if>

    return `${urlPrefix}#${this.entry.key}`;
  }

  private computeCollabsible_(): boolean {
    return this.entry.value.length > COLLAPSE_THRESHOLD;
  }

  private onButtonClick_() {
    assert(this.collapsible_);
    this.collapsed = !this.collapsed;
  }

  private getButtonText_(): string {
    return this.collapsed ? 'Expand…' : 'Collapse…';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'key-value-pair-entry': KeyValuePairEntryElement;
  }
}

customElements.define(KeyValuePairEntryElement.is, KeyValuePairEntryElement);
