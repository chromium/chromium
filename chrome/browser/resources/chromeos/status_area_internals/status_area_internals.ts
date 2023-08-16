// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import './privacy_indicator_app_manager.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './status_area_internals.html.js';

/**
 * @fileoverview
 * 'status-area-internals' defines the UI for the "ChromeOS Status Area
 * Internals" test page.
 */

export class StatusAreaInternalsElement extends PolymerElement {
  static get is() {
    return 'status-area-internals';
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

customElements.define(
    StatusAreaInternalsElement.is, StatusAreaInternalsElement);
