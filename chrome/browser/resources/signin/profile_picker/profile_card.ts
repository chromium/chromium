// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './profile_card_menu.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ManageProfilesBrowserProxy, ProfileState} from './manage_profiles_browser_proxy.js';
import {ManageProfilesBrowserProxyImpl} from './manage_profiles_browser_proxy.js';
import {getCss} from './profile_card.css.js';
import {getHtml} from './profile_card.html.js';
import {createDummyProfileState} from './profile_picker_util.js';

export interface ProfileCardElement {
  $: {
    gaiaName: HTMLElement,
    gaiaNameTooltip: CrTooltipElement,
    nameInput: CrInputElement,
    tooltip: CrTooltipElement,
    profileCardButton: CrButtonElement,
  };
}

const ProfileCardElementBase = I18nMixinLit(CrLitElement);

export class ProfileCardElement extends ProfileCardElementBase {
  static get is() {
    return 'profile-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      profileState: {type: Object},
      pattern_: {type: String},
    };
  }

  profileState: ProfileState = createDummyProfileState();
  protected pattern_: string = '.*\\S.*';
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addNameInputTooltipListeners_();
    this.addGaiaNameTooltipListeners_();

    this.addEventListener('drag-tile-start', this.disableActiveRipple_);
  }

  private addNameInputTooltipListeners_() {
    const showTooltip = () => {
      const target = this.$.tooltip.target;
      assert(target);
      const inputElement = (target as CrInputElement).inputElement;
      // Disable tooltip if the local name editing is in progress.
      if (this.isNameTruncated_(inputElement) &&
          !this.$.nameInput.hasAttribute('focused_')) {
        this.$.tooltip.show();
        return;
      }
      this.$.tooltip.hide();
    };
    const hideTooltip = () => this.$.tooltip.hide();
    const target = this.$.tooltip.target;
    assert(target);
    target.addEventListener('mouseenter', showTooltip);
    target.addEventListener('focus', hideTooltip);
    target.addEventListener('mouseleave', hideTooltip);
    target.addEventListener('click', hideTooltip);
    this.$.tooltip.addEventListener('mouseenter', hideTooltip);
  }

  private addGaiaNameTooltipListeners_() {
    const showTooltip = () => {
      if (this.isNameTruncated_(this.$.gaiaName)) {
        this.$.gaiaNameTooltip.show();
        return;
      }
      this.$.gaiaNameTooltip.hide();
    };
    const hideTooltip = () => this.$.gaiaNameTooltip.hide();
    const target = this.$.gaiaNameTooltip.target;
    assert(target);
    target.addEventListener('mouseenter', showTooltip);
    target.addEventListener('focus', showTooltip);
    target.addEventListener('mouseleave', hideTooltip);
    target.addEventListener('blur', hideTooltip);
    target.addEventListener('tap', hideTooltip);
    this.$.gaiaNameTooltip.addEventListener('mouseenter', hideTooltip);
  }

  private isNameTruncated_(element: HTMLElement): boolean {
    return !!element && element.scrollWidth > element.offsetWidth;
  }

  protected onProfileClick_() {
    this.manageProfilesBrowserProxy_.launchSelectedProfile(
        this.profileState.profilePath);
  }

  protected onNameInputPointerEnter_() {
    this.dispatchEvent(new CustomEvent(
        'toggle-drag', {composed: true, detail: {toggle: false}}));
  }

  protected onNameInputPointerLeave_() {
    this.dispatchEvent(new CustomEvent(
        'toggle-drag', {composed: true, detail: {toggle: true}}));
  }

  /**
   * Handler for when the profile name field is changed, then blurred.
   */
  protected onProfileNameChanged_(event: Event) {
    const target = event.target as CrInputElement;

    if (target.invalid) {
      return;
    }

    this.manageProfilesBrowserProxy_.setProfileName(
        this.profileState.profilePath, target.value);

    target.blur();
  }

  /**
   * Handler for profile name keydowns.
   */
  protected onProfileNameKeydown_(event: KeyboardEvent) {
    if (event.key === 'Escape' || event.key === 'Enter') {
      (event.target as HTMLElement).blur();
    }
  }

  /**
   * Handler for profile name blur.
   */
  protected onProfileNameInputBlur_() {
    if (this.$.nameInput.invalid) {
      this.$.nameInput.value = this.profileState.localProfileName;
    }
  }

  /**
   * Disables the ripple effect if any. This is needed when the tile is being
   * dragged in order not to break the visual effect of the dragging tile and
   * mouse positioning relative to the card.
   */
  private disableActiveRipple_(): void {
    if (this.$.profileCardButton.hasRipple()) {
      const buttonRipple = this.$.profileCardButton.getRipple();
      // This sequence is equivalent to calling `buttonRipple.clear()` but also
      // avoids the animation effect which is needed in this case.
      buttonRipple.showAndHoldDown();
      buttonRipple.holdDown = false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-card': ProfileCardElement;
  }
}

customElements.define(ProfileCardElement.is, ProfileCardElement);
