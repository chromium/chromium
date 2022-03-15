// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './strings.m.js';
import './signin_shared_css.js';
import './signin_vars_css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ProfileCustomizationBrowserProxy, ProfileCustomizationBrowserProxyImpl, ProfileInfo} from './profile_customization_browser_proxy.js';


export interface ProfileCustomizationAppElement {
  $: {
    doneButton: CrButtonElement,
    nameInput: CrInputElement,
    title: HTMLElement,
  };
}

const ProfileCustomizationAppElementBase = WebUIListenerMixin(PolymerElement);

export class ProfileCustomizationAppElement extends
    ProfileCustomizationAppElementBase {
  static get is() {
    return 'profile-customization-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Whether the account is managed (Enterprise) */
      isManaged_: {
        type: Boolean,
        value: false,
      },

      /** Local profile name, editable by user input */
      profileName_: {
        type: String,
        value: '',
      },

      /** URL for the profile picture */
      pictureUrl_: String,

      /** Welcome title for the bubble */
      welcomeTitle_: String,
    };
  }

  private isManaged_: boolean;
  private profileName_: string;
  private pictureUrl_: string;
  private welcomeTitle_: string;
  private profileCustomizationBrowserProxy_: ProfileCustomizationBrowserProxy =
      ProfileCustomizationBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    // profileName_ is only set now, because it triggers a validation of the
    // input which crashes if it's done too early.
    this.profileName_ = loadTimeData.getString('profileName');
    this.addWebUIListener(
        'on-profile-info-changed',
        (info: ProfileInfo) => this.setProfileInfo_(info));
    this.profileCustomizationBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
  }

  /**
   * Called when the Done button is clicked. Sends the profile name back to
   * native.
   */
  private onDoneCustomizationClicked_() {
    this.profileCustomizationBrowserProxy_.done(this.profileName_);
  }

  private isDoneButtonDisabled_(): boolean {
    return !this.profileName_ || !this.$.nameInput.validate();
  }

  private setProfileInfo_(profileInfo: ProfileInfo) {
    this.style.setProperty(
        '--header-background-color', profileInfo.backgroundColor);
    this.pictureUrl_ = profileInfo.pictureUrl;
    this.isManaged_ = profileInfo.isManaged;
    this.welcomeTitle_ = profileInfo.welcomeTitle;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-customization-app': ProfileCustomizationAppElement;
  }
}

customElements.define(
    ProfileCustomizationAppElement.is, ProfileCustomizationAppElement);
