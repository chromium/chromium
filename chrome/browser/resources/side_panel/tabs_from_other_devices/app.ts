// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';

const TabsFromOtherDevicesAppElementBase = CrLitElement;

export class TabsFromOtherDevicesAppElement extends
    TabsFromOtherDevicesAppElementBase {
  static get is() {
    return 'tabs-from-other-devices-app';
  }

  // TODO(crbug.com/488252161): Add actual contents.

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabs-from-other-devices-app': TabsFromOtherDevicesAppElement;
  }
}

customElements.define(
    TabsFromOtherDevicesAppElement.is, TabsFromOtherDevicesAppElement);
