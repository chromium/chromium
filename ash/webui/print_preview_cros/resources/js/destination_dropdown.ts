// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../css/print_preview_cros_shared.css.js';
import './destination_row.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './destination_dropdown.html.js';
import {DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED, DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, DestinationDropdownController} from './destination_dropdown_controller.js';
import {type DestinationRowElement} from './destination_row.js';
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
      destinations: Array,
      disabled: {
        type: Boolean,
        reflectToAttribute: true,
      },
      open: Boolean,
      selectedDestination: Object,
    };
  }

  disabled: boolean;
  private controller: DestinationDropdownController;
  private eventTracker = new EventTracker();
  private destinations: Destination[] = [];
  private open = false;
  private selectedDestination: Destination|null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.controller = new DestinationDropdownController(this.eventTracker);


    this.eventTracker.add(
        this.controller, DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED,
        (): void => this.onDestinationDropdownDisabledChanged());
    this.eventTracker.add(
        this.controller, DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION,
        (): void => this.onDestinationDropdownUpdateSelectedDestination());
    this.eventTracker.add(
        this.controller, DESTINATION_DROPDOWN_UPDATE_DESTINATIONS,
        (): void => this.onDestinationDropdownUpdateDestinations());

    // Initialize properties using the controller.
    this.selectedDestination = this.controller.getSelectedDestination();
    this.destinations = this.controller.getDestinations();
    this.disabled = this.controller.shouldDisableDropdown();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  // Handles toggling visibility of dropdown content.
  onSelectedClicked(): void {
    if (this.disabled) {
      return;
    }

    this.open = !this.open;
  }

  // Handles passing the ID of the destination row that was clicked to the
  // controller for processing and closes the dropdown menu.
  onDestinationClicked(event: Event): void {
    assert(event.target);
    const row: DestinationRowElement = event.target as DestinationRowElement;
    assert(row.destination);

    // Clicked on currently selected destination.
    if (this.selectedDestination?.id === row.destination.id) {
      this.open = false;
      return;
    }

    const destinationUpdated =
        this.controller.updateActiveDestination(row.destination!.id);
    if (destinationUpdated) {
      // Immediately update UI to display selected destination.
      this.selectedDestination = row.destination;
    }

    this.open = false;
  }

  // Handles updating disabled state.
  private onDestinationDropdownDisabledChanged(): void {
    this.disabled = this.controller.shouldDisableDropdown();

    // Ensure content is closed if control disabled to avoid content being
    // locked open.
    if (this.disabled) {
      this.open = false;
    }
  }

  // Handles updating dropdown UI content when update content event occurs.
  private onDestinationDropdownUpdateDestinations(): void {
    // Use spread operator to ensure Polymer registers array update.
    this.destinations = [...this.controller.getDestinations()];
  }

  // Handles updating UI when update selected destination event occurs.
  private onDestinationDropdownUpdateSelectedDestination(): void {
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
