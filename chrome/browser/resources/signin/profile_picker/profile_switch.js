// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './profile_picker_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';

/** @polymer */
export class ProfileSwitchElement extends PolymerElement {
  static get is() {
    return 'profile-switch';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {ProfileState} */
      profileState_: {
        type: Object,
      },

      /** @type {boolean} */
      isProfileStateInitialized_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /** @private {?ManageProfilesBrowserProxy} */
    this.manageProfilesBrowserProxy_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    this.manageProfilesBrowserProxy_ =
        ManageProfilesBrowserProxyImpl.getInstance();
    this.manageProfilesBrowserProxy_.getSwitchProfile().then(profileState => {
      this.profileState_ = profileState;
      this.isProfileStateInitialized_ = true;
    });
  }

  /** @private */
  onCancelClick_() {
    this.manageProfilesBrowserProxy_.cancelProfileSwitch();
  }

  /** @private */
  onSwitchClick_() {
    this.manageProfilesBrowserProxy_.confirmProfileSwitch(
        this.profileState_.profilePath);
  }
}

customElements.define(ProfileSwitchElement.is, ProfileSwitchElement);
