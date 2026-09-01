// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './readonly_omnibox.js';
import './location_icon.js';
import './content_settings_icons.js';
import './page_action_icons.js';
import './selected_keyword.js';
import '/shared/permission_dashboard.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';
import {SecurityChipRole} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {LocationBarState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {PageActionId} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import type {ToolbarAppElement} from './app.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './location_bar.css.js';
import {getHtml} from './location_bar.html.js';
import type {PageActionIconsElement} from './page_action_icons.js';
import type {ReadonlyOmniboxElement} from './readonly_omnibox.js';
import type {ResponsiveControl} from './responsive_control.js';
import {PressHandler} from './toolbar_button.js';

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
      touchUi: {type: Boolean},
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
      a11yFriendlySuggestionText: '',
      selection: null,
      textIsUrl: false,
      userInputInProgress: false,
    },
    locationBarFlags: {
      userInputInProgress: false,
      popupOpen: false,
      forceAimButtonFocusRing: false,
      isVirtualKeyboardVisible: false,
    },
    selectedKeyword: null,
    lhsChipsState: {
      securityChip: {
        icon: {handleId: 0n},
        securityLevel: 0,
        text: '',
        tooltip: '',
        accessibilityState: {
          role: SecurityChipRole.kButton,
          label: '',
          description: '',
        },
        isClickable: false,
        isTextDangerous: false,
        isVisible: true,
        isContextMenuVisible: false,
      },
      activityIndicators: [],
      permissionDashboard: null,
    },
    contentSettingImageStates: [],
    pageActionStates: [],
  };

  accessor isPopupOpen: boolean = false;
  accessor touchUi: boolean = false;

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private focusState_: boolean = false;

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();
    // Need to use focusin/focusout and not focus/blur here since we
    // specifically want the events from child elements.
    this.addEventListener('focusin', this.onFocusin_.bind(this));
    this.addEventListener('focusout', this.onFocusout_.bind(this));
    // We also need blur for document losing focus.
    this.addEventListener('blur', this.onBlur_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
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
      const pageActions = this.locationBarState.pageActionStates;
      const isAimLastVisible = pageActions && pageActions.length === 1 &&
          pageActions[0]?.pageActionId === PageActionId.kActionAiMode;
      this.classList.toggle('aim-last-page-action', isAimLastVisible);

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
   * Calculates the maximum width of the location bar's content area, taking
   * into account the current size of all other controls on the toolbar and the
   * width of the window. Note that this is available width in a CSS sense, so,
   * e.g., exterior margins are not included in the return value. Requires the
   * location bar be displayed to accurately calculate this value.
   *
   * To achieve this without replicating CSS layout calculations (margins,
   * padding, gaps, child visibility, walking through children), it adds the
   * `ToolbarAppElement.getAvailableWidth()` to current width of the location
   * bar.
   *
   * Always returns a value of at least LOCATION_BAR_MIN_WIDTH, even if there's
   * not that much width available.
   */
  private getMaxAvailableWidth(): number {
    const shadowRoot = this.getRootNode() as ShadowRoot;
    if (!shadowRoot || !shadowRoot.host) {
      return 0;
    }
    const toolbarApp = shadowRoot.host as ToolbarAppElement;
    const availableWidth = toolbarApp.getAvailableWidth() + this.clientWidth;
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
  // allocated to the location bar by calling setToMaxAvailableWidth(),
  // potentially increasing its size beyond its preferred width.
  expandUpToPreferredWidth() {
    const width = Math.min(
        this.getMaxAvailableWidth(),
        LocationBarElement.LOCATION_BAR_PREFERRED_WIDTH);
    this.style.width = `${width}px`;
  }

  controlsToAddToOverflowMenu(): OverflowMenuItem[] {
    return [];
  }

  // Sets the width to include all remaining unclaimed space on the toolbar.
  // Will not set to less than minimum width.
  //
  // TODO(crbug.com/491791965): Should we shrink the location bar to even less
  // than the minimum if there's less width than that available?
  setToMaxAvailableWidth() {
    this.style.width = `${this.getMaxAvailableWidth()}px`;
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

  protected getClearButtonTitle_(): string {
    return loadTimeData.getString('clearButtonTooltip');
  }

  protected getClearButtonIcon_(): string {
    return this.touchUi ? 'webui-toolbar:backspace_filled' :
                          'webui-toolbar:close';
  }

  protected shouldShowClearButton_(): boolean {
    return this.locationBarState.locationBarFlags.userInputInProgress &&
        this.locationBarState.omniboxViewState.textPieces.some(
            piece => piece.text.length > 0) &&
        this.locationBarState.locationBarFlags.isVirtualKeyboardVisible;
  }

  protected clearPressHandler_: PressHandler = new PressHandler(
      /*onLongPress=*/ () => {},
      /*onShortPress=*/ () => this.clearInput_(),
      /*enableContextMenu=*/ false,
  );

  private clearInput_() {
    this.$.omnibox.clearInput();
  }

  protected onClearClick_(e: MouseEvent) {
    // Only keyboard `click` (Enter/Space) are handled here, which triggers a
    // left-click equivalent. Keyboard 'click' has detail === 0.
    if (e.detail === 0) {
      this.clearInput_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'location-bar': LocationBarElement;
  }
}

customElements.define(LocationBarElement.is, LocationBarElement);
