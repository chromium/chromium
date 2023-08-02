// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './profile_card_menu.js';
import './profile_picker_shared.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {getTemplate} from './profile_card.html.js';

export interface ProfileCardElement {
  $: {
    gaiaName: HTMLElement,
    gaiaNameTooltip: PaperTooltipElement,
    nameInput: CrInputElement,
    tooltip: PaperTooltipElement,
  };
}

const ProfileCardElementBase = I18nMixin(PolymerElement);

export class ProfileCardElement extends ProfileCardElementBase {
  static get is() {
    return 'profile-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profileState: {
        type: Object,
      },

      pattern_: {
        type: String,
        value: '.*\\S.*',
      },
    };
  }

  profileState: ProfileState;
  private pattern_: string;
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addNameInputTooltipListeners_();
    this.addGaiaNameTooltipListeners_();
  }

  private addNameInputTooltipListeners_() {
    const showTooltip = () => {
      const inputElement = this.$.tooltip.target.inputElement;
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

  private onProfileClick_() {
    this.manageProfilesBrowserProxy_.launchSelectedProfile(
        this.profileState.profilePath);
  }

  private onNameInputPointerEnter_() {
    this.dispatchEvent(new CustomEvent(
        'toggle-drag', {composed: true, detail: {toggle: false}}));
  }

  private onNameInputPointerLeave_() {
    this.dispatchEvent(new CustomEvent(
        'toggle-drag', {composed: true, detail: {toggle: true}}));
  }

  /**
   * Handler for when the profile name field is changed, then blurred.
   */
  private onProfileNameChanged_(event: Event) {
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
  private onProfileNameKeydown_(event: KeyboardEvent) {
    if (event.key === 'Escape' || event.key === 'Enter') {
      (event.target as HTMLElement).blur();
    }
  }

  /**
   * Handler for profile name blur.
   */
  private onProfileNameInputBlur_() {
    if (this.$.nameInput.invalid) {
      this.$.nameInput.value = this.profileState.localProfileName;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-card': ProfileCardElement;
  }
}

customElements.define(ProfileCardElement.is, ProfileCardElement);
