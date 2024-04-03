// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../css/print_preview_cros_shared.css.js';
import './destination_row.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './destination_dropdown.html.js';
import {DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, DestinationDropdownController} from './destination_dropdown_controller.js';
import {Destination} from './utils/print_preview_cros_app_types.js';


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

  static get properties() {
    return {
      selectedDestination: Object,
    };
  }

  private controller: DestinationDropdownController;
  private eventTracker = new EventTracker();
  private selectedDestination: Destination|null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.controller = new DestinationDropdownController(this.eventTracker);

    this.eventTracker.add(
        this.controller, DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION,
        (e: Event): void =>
            this.onDestinationDropdownUpdateSelectedDestination(e));

    // Initialize properties using the controller.
    this.selectedDestination = this.controller.getSelectedDestination();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  // Handles updating UI when update selected destination event occurs.
  private onDestinationDropdownUpdateSelectedDestination(_e: Event): void {
    this.selectedDestination = this.controller.getSelectedDestination();
  }

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
