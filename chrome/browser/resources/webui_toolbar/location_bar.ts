// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './readonly_omnibox.js';
import './location_icon.js';
import './content_settings_icons.js';
import './page_action_icons.js';
import './selected_keyword.js';
import '/shared/permission_dashboard.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {LocationBarState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './location_bar.css.js';
import {getHtml} from './location_bar.html.js';
import type {PageActionIconsElement} from './page_action_icons.js';
import type {ReadonlyOmniboxElement} from './readonly_omnibox.js';

export interface LocationBarElement {
  $: {
    omnibox: ReadonlyOmniboxElement,
    pageActions: PageActionIconsElement,
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
      isPopupOpen: {type: Boolean},
    };
  }

  accessor locationBarState: LocationBarState = {
    omniboxViewState: {
      browserVersion: 0,
      uiVersion: 0,
      formattedFullUrl: '',
      textPieces: [],
      placeholder: null,
      inlineAutocompletion: '',
      additionalText: '',
      selection: null,
      textIsUrl: false,
      userInputInProgress: false,
    },
    locationBarFlags: {
      userInputInProgress: false,
      popupOpen: false,
      forceAimButtonFocusRing: false,
    },
    selectedKeyword: null,
    lhsChipsState: {
      securityChip: {
        icon: {handleId: 0n},
        securityLevel: 0,
        text: '',
        accessibilityState: {
          label: '',
          description: '',
        },
        isClickable: false,
        isTextDangerous: false,
        isVisible: true,
      },
      activityIndicators: [],
      permissionDashboard: null,
    },
    contentSettingImageStates: [],
    pageActionStates: [],
  };

  accessor isPopupOpen: boolean = false;

  private trackedElementManager_: TrackedElementManager;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private focusState_: boolean = false;

  constructor() {
    super();
    this.trackedElementManager_ = TrackedElementManager.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.trackedElementManager_.startTracking(
        this.$.omnibox, 'kOmniboxElementId');
    // Need to use focusin/focusout and not focus/blur here since we
    // specifically want the events from child elements.
    this.addEventListener('focusin', this.onFocusin_.bind(this));
    this.addEventListener('focusout', this.onFocusout_.bind(this));
    // We also need blur for document losing focus.
    this.addEventListener('blur', this.onBlur_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.trackedElementManager_.stopTracking(this.$.omnibox);
  }

  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);
    if (changedProperties.has('locationBarState')) {
      this.isPopupOpen = this.locationBarState.locationBarFlags.popupOpen;
    }
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('locationBarState')) {
      this.classList.toggle(
          'popup-open', this.locationBarState.locationBarFlags.popupOpen);
      this.classList.toggle(
          'input-in-progress',
          this.locationBarState.locationBarFlags.userInputInProgress);
      this.classList.toggle(
          'no-focus-ring',
          this.locationBarState.locationBarFlags.popupOpen ||
              this.locationBarState.locationBarFlags.forceAimButtonFocusRing);
      const aimButton = this.$.pageActions.aiModePageAction();
      if (aimButton) {
        aimButton.forceFocusRing =
            this.locationBarState.locationBarFlags.forceAimButtonFocusRing;
      }
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

  private onFocusin_() {
    this.updateFocusWithin_();
  }

  private onFocusout_() {
    this.updateFocusWithin_();
  }

  private onBlur_() {
    this.updateFocusWithin_();
  }

  private updateFocusWithin_() {
    const hasFocus =
        document.hasFocus() && (this.shadowRoot.activeElement !== null);
    if (hasFocus !== this.focusState_) {
      this.focusState_ = hasFocus;
      this.browserProxy_.toolbarUIHandler.onLocationBarFocusWithinChanged(
          hasFocus);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'location-bar': LocationBarElement;
  }
}

customElements.define(LocationBarElement.is, LocationBarElement);
