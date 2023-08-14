// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './status_area_tester.html.js';

/**
 * @fileoverview
 * 'status-area-tester' defines the UI for the "ChromeOS Status Area Tester"
 * test page.
 */

export class StatusAreaTesterElement extends PolymerElement {
  static get is() {
    return 'status-area-tester';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onImeToggled_(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    chrome.send('toggleIme', [toggled]);
  }

  private onPaletteToggled_(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    chrome.send('togglePalette', [toggled]);
  }
}

customElements.define(StatusAreaTesterElement.is, StatusAreaTesterElement);
