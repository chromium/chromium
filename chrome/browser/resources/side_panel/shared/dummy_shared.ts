// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dummy shared element to demonstrate how to share UI components
 * between side panels.
 * TODO(dpapad): Delete once the first non-dummy shared element is added.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './dummy_shared.html.js';

export class DummySharedElement extends PolymerElement {
  static get is() {
    return 'dummy-shared';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'dummy-shared': DummySharedElement;
  }
}

customElements.define(DummySharedElement.is, DummySharedElement);
