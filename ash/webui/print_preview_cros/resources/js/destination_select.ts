// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './destination_dropdown.js';
import '../css/print_preview_cros_shared.css.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './destination_select.html.js';
import {DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED, DestinationSelectController} from './destination_select_controller.js';

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
      showLoadingUi: Boolean,
    };
  }

  private controller: DestinationSelectController;
  private eventTracker = new EventTracker();
  private showLoadingUi: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    this.controller = new DestinationSelectController(this.eventTracker);

    this.eventTracker.add(
        this.controller, DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED,
        (): void => this.onDestinationSelectShowDropdownChanged());

    // Initialize properties using the controller.
    this.showLoadingUi = this.controller.shouldShowLoadingUi();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  getControllerForTesting(): DestinationSelectController {
    return this.controller;
  }

  // Updates UI on controller DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED event.
  private onDestinationSelectShowDropdownChanged(): void {
    this.showLoadingUi = this.controller.shouldShowLoadingUi();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DestinationSelectElement.is]: DestinationSelectElement;
  }
}

customElements.define(DestinationSelectElement.is, DestinationSelectElement);
