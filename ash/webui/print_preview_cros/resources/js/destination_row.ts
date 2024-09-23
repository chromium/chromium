// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './destination_row.html.js';
import {DestinationRowController} from './destination_row_controller.js';
import {Destination} from './utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'destination-row' handles the display of destination properties such as
 * the device name, status, etc.
 */

export class DestinationRowElement extends PolymerElement {
  static get is() {
    return 'destination-row' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destination: Object,
    };
  }

  private controller = new DestinationRowController();
  destination: Destination|null = null;

  getControllerForTesting(): DestinationRowController {
    return this.controller;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DestinationRowElement.is]: DestinationRowElement;
  }
}

customElements.define(DestinationRowElement.is, DestinationRowElement);
