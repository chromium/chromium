// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './destination_dropdown.html.js';
import {DestinationDropdownController} from './destination_dropdown_controller.js';


/**
 * @fileoverview
 * 'destination-dropdown' displays the print job selected destination when the
 * dropdown is closed. When the dropdown is open it displays a list of digital
 * and recent destinations as well as an option to open the destination dialog.
 */

export class DestinationDropdownElement extends PolymerElement {
  static get is() {
    return 'destination-dropdown' as const;
  }

  static get template() {
    return getTemplate();
  }

  private controller = new DestinationDropdownController();

  getControllerForTesting(): DestinationDropdownController {
    return this.controller;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DestinationDropdownElement.is]: DestinationDropdownElement;
  }
}

customElements.define(
    DestinationDropdownElement.is, DestinationDropdownElement);
