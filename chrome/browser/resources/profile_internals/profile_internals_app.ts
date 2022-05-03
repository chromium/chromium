// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_internals_app.html.js';
import {ProfileInternalsBrowserProxy, ProfileInternalsBrowserProxyImpl, ProfileState} from './profile_internals_browser_proxy.js';

const ProfileInternalsAppElementBase = WebUIListenerMixin(PolymerElement);

export class ProfileInternalsAppElement extends ProfileInternalsAppElementBase {
  static get is() {
    return 'profile-internals-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Profiles list supplied by ProfileInternalsBrowserProxy.
       */
      profilesList_: {type: Array, value: () => []},
    };
  }

  private profileInternalsBrowserProxy_: ProfileInternalsBrowserProxy =
      ProfileInternalsBrowserProxyImpl.getInstance();
  private profilesList_: Array<ProfileState>;

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUIListener(
        'profiles-list-changed',
        (profilesList: Array<ProfileState>) =>
            this.handleProfilesListChanged_(profilesList));
    this.profileInternalsBrowserProxy_.getProfilesList();
  }

  /**
   * Handler for when the profiles list are updated.
   */
  private handleProfilesListChanged_(profilesList: Array<ProfileState>) {
    this.profilesList_ = profilesList;
  }
}

customElements.define(
    ProfileInternalsAppElement.is, ProfileInternalsAppElement);
