// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './strings.m.js';

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_internals_app.html.js';
import {ProfileInternalsBrowserProxy, ProfileInternalsBrowserProxyImpl, ProfileState, ProfileStateElement} from './profile_internals_browser_proxy.js';

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
  private profilesList_: Array<ProfileStateElement>;

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
    const profilesExpanded = new Map(this.profilesList_.map(
        item => [item.profileState.profilePath, item.expanded]));
    this.profilesList_ = profilesList.map(
        profile => ({
          profileState: profile,
          expanded: profilesExpanded.get(profile.profilePath) ?? false,
        }));
  }
}

customElements.define(
    ProfileInternalsAppElement.is, ProfileInternalsAppElement);
