// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './destination_select.html.js';
import {DestinationSelectController} from './destination_select_controller.js';

/**
 * @fileoverview
 * 'destination-select' is a wrapper for the elements related to updating the
 * print destination and providing messaging around destination availability.
 */

export class DestinationSelectElement extends PolymerElement {
  static get is() {
    return 'destination-select' as const;
  }

  static get template() {
    return getTemplate();
  }

  private controller = new DestinationSelectController();

  getControllerForTesting(): DestinationSelectController {
    return this.controller;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DestinationSelectElement.is]: DestinationSelectElement;
  }
}

customElements.define(DestinationSelectElement.is, DestinationSelectElement);
