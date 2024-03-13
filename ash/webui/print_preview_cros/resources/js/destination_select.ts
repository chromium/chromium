// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './destination_dropdown.js';

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

  static get properties() {
    return {
      showLoading: Boolean,
    };
  }

  private controller = new DestinationSelectController();
  private showLoading: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    // Initialize properties using the controller.
    this.showLoading = this.controller.shouldShowLoading();
  }

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
