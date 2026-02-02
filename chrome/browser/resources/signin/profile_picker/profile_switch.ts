// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ManageProfilesBrowserProxy, ProfileState} from './manage_profiles_browser_proxy.js';
import {ManageProfilesBrowserProxyImpl} from './manage_profiles_browser_proxy.js';
import {createDummyProfileState} from './profile_picker_util.js';
import {getCss} from './profile_switch.css.js';
import {getHtml} from './profile_switch.html.js';

export interface ProfileSwitchElement {
  $: {
    iconContainer: HTMLElement,
    profileName: HTMLElement,
    gaiaName: HTMLElement,
    cancelButton: CrButtonElement,
    switchButton: CrButtonElement,
  };
}

export class ProfileSwitchElement extends CrLitElement {
  static get is() {
    return 'profile-switch';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      profileState_: {type: Object},
      isProfileStateInitialized_: {type: Boolean},
      usePrimaryAndTonalButtons_: {type: Boolean},
    };
  }

  protected accessor profileState_: ProfileState = createDummyProfileState();
  protected accessor isProfileStateInitialized_: boolean = false;
  private accessor usePrimaryAndTonalButtons_: boolean =
      loadTimeData.getBoolean('usePrimaryAndTonalButtonsForPromos');
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  override firstUpdated() {
    const params = new URLSearchParams(window.location.search);
    const profileSwitchPath = params.get('profileSwitchPath');
    assert(profileSwitchPath !== null, '`profileSwitchPath` param is not set');
    this.manageProfilesBrowserProxy_.getProfileState(profileSwitchPath)
        .then(profileState => {
          this.profileState_ = profileState;
          this.isProfileStateInitialized_ = true;
        });
  }

  protected onCancelClick_() {
    this.manageProfilesBrowserProxy_.cancelProfileSwitch();
  }

  protected onSwitchClick_() {
    this.manageProfilesBrowserProxy_.confirmProfileSwitch(
        this.profileState_.profilePath);
  }

  protected getCancelButtonClass_(): string {
    return this.usePrimaryAndTonalButtons_ ? 'tonal-button' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-switch': ProfileSwitchElement;
  }
}

customElements.define(ProfileSwitchElement.is, ProfileSwitchElement);
