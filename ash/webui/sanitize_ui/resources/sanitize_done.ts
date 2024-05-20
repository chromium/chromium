// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sanitize_done.html.js';

export interface SanitizeDoneElement {
  $: {
    header: HTMLDivElement,
  };
}

/**
 * @fileoverview
 * 'sanitize-done' is a dialog shown after reverting to safe settings
 * (aka sanitize).
 */
export class SanitizeDoneElement extends PolymerElement {
  static get is() {
    return 'sanitize-done' as const;
  }

  static get template() {
    return getTemplate();
  }

  override ready() {
    super.ready();
    this.$.header.textContent = 'Sanitize Done';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SanitizeDoneElement.is]: SanitizeDoneElement;
  }
}
customElements.define(SanitizeDoneElement.is, SanitizeDoneElement);
