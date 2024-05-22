// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './key_value_pair_entry.css.js';
import {getHtml} from './key_value_pair_entry.html.js';

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

export class KeyValuePairEntryElement extends CrLitElement {
  static get is() {
    return 'key-value-pair-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entry: {type: Object},

      collapsible_: {
        type: Boolean,
        reflect: true,
      },

      collapsed: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  entry: KeyValuePairEntry = {key: '', value: ''};
  collapsed: boolean = true;
  protected collapsible_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('entry')) {
      this.collapsible_ = this.entry.value.length > COLLAPSE_THRESHOLD;
    }
  }

  protected getHref_(): string {
    // Let URL be anchor to the section of this page by default.
    let urlPrefix = '';

    // <if expr="chromeos_ash">
    // Link to the markdown doc with documentation for the entry for Chrome OS
    // instead.
    urlPrefix = CROS_MD_DOC_URL;
    // </if>

    return `${urlPrefix}#${this.entry.key}`;
  }

  protected onButtonClick_() {
    assert(this.collapsible_);
    this.collapsed = !this.collapsed;
  }

  protected getButtonText_(): string {
    return this.collapsed ? 'Expand…' : 'Collapse…';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'key-value-pair-entry': KeyValuePairEntryElement;
  }
}

customElements.define(KeyValuePairEntryElement.is, KeyValuePairEntryElement);
