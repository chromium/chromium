// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './readonly_omnibox.js';
import './location_icon.js';
import './content_settings_icons.js';
import './permission_dashboard.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './location_bar.css.js';
import {getHtml} from './location_bar.html.js';
import {type ReadonlyOmniboxElement} from './readonly_omnibox.js';
import type {LocationBarState} from './toolbar_ui_api_data_model.mojom-webui.js';

export interface LocationBarElement {
  $: {
    omnibox: ReadonlyOmniboxElement,
  };
}

export class LocationBarElement extends CrLitElement {
  static get is() {
    return 'location-bar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      locationBarState: {type: Object},
    };
  }

  accessor locationBarState: LocationBarState = {
    omniboxViewState: {
      textPieces: [],
      inlineAutocompletion: '',
      selection: null,
      textIsUrl: false,
    },
    locationBarFlags: {
      userInputInProgress: false,
      popupOpen: false,
    },
    lhsChipsState: {
      securityChip: {
        icon: 0,
        securityLevel: 0,
        text: '',
        isClickable: false,
        isTextDangerous: false,
        isVisible: true,
      },
      activityIndicators: [],
      permissionDashboard: null,
    },
    contentSettingImageStates: [],
  };

  private trackedElementManager_: TrackedElementManager;

  constructor() {
    super();
    this.trackedElementManager_ = TrackedElementManager.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.trackedElementManager_.startTracking(
        this.$.omnibox, 'kOmniboxElementId');
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.trackedElementManager_.stopTracking(this.$.omnibox);
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('locationBarState')) {
      this.classList.toggle(
          'popup-open', this.locationBarState.locationBarFlags.popupOpen);
      this.classList.toggle(
          'input-in-progress',
          this.locationBarState.locationBarFlags.userInputInProgress);
    }
  }

  protected onChipPointerenter_() {
    this.toggleAttribute('chip-hovered', true);
  }

  protected onChipPointerleave_() {
    this.toggleAttribute('chip-hovered', false);
  }

  protected onChipPointercancel_() {
    this.onChipPointerleave_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'location-bar': LocationBarElement;
  }
}

customElements.define(LocationBarElement.is, LocationBarElement);
