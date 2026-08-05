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
import type {ResponsiveControl} from './responsive_control.js';

export interface LocationBarElement {
  $: {
    omnibox: ReadonlyOmniboxElement,
    pageActions: PageActionIconsElement,
  };
}

export class LocationBarElement extends CrLitElement implements
    ResponsiveControl {
  // The smallest allowed width of the location bar.
  //
  // TODO(crbug.com/474060468): This is a placeholder value. We need to do a
  // proper calculation.
  static readonly LOCATION_BAR_MIN_WIDTH = 330;
  // The preferred width of the location bar. It will, based on priority order,
  // try to assume this width when ResponsiveControls are all being sized. At
  // the end of that process, expandUpToPreferredWidth() will be invoked, and it
  // will claim any extra available width.
  //
  // TODO(crbug.com/474060468): This is a placeholder value. We need to do a
  // proper calculation.
  static readonly LOCATION_BAR_PREFERRED_WIDTH = 400;

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

  /**
   * Calculates the remaining available width for the location bar's content
   * area. Returned available width includes the width currently taken up by the
   * location bar. Note that this is available width in a CSS sense, so, e.g.,
   * exterior margins are not included in the return value. Requires the
   * location bar be displayed to accurately calculate this value.
   *
   * To achieve this without replicating CSS layout calculations (margins,
   * padding, gaps, child visibility, walking through children), it takes the
   * inner width of the window, subtracts the current width of the parent
   * element, which should be the toolbar itself, and then adds back the current
   * width of the location bar.
   */
  private getAvailableWidth(): number {
    const shadowRoot = this.getRootNode() as ShadowRoot;
    if (!shadowRoot || !shadowRoot.host) {
      return 0;
    }
    const host = shadowRoot.host as HTMLElement;
    const availableWidth =
        window.innerWidth - host.clientWidth + this.clientWidth;
    // Always consider at least the minimum required width available.
    return Math.max(availableWidth, LocationBarElement.LOCATION_BAR_MIN_WIDTH);
  }

  // ResponsiveControl implementation
  shouldBeShown(): boolean {
    return true;
  }

  setToMinWidth() {
    this.style.width = `${LocationBarElement.LOCATION_BAR_MIN_WIDTH}px`;
  }

  setToPreferredWidth() {
    this.style.width = `${LocationBarElement.LOCATION_BAR_PREFERRED_WIDTH}px`;
  }

  // For the location bar, the "preferred width" is maximum width the location
  // bar will assume before space is allocated to lower priority
  // ResponsiveControls. At the end of layout, any remaining available space is
  // allocated to the location bar by calling setToAvailableWidth(), potentially
  // increasing its size beyond its preferred width.
  expandUpToPreferredWidth() {
    const width = Math.min(
        this.getAvailableWidth(),
        LocationBarElement.LOCATION_BAR_PREFERRED_WIDTH);
    this.style.width = `${width}px`;
  }

  consumeNeedsLayout(): boolean {
    // The minimum and preferred sizes of this control do not change.
    return false;
  }

  // Sets size to include all remaining unclaimed space on the toolbar.
  setToAvailableWidth() {
    this.style.width = `${this.getAvailableWidth()}px`;
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
