// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './strings.m.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_internals_app.html.js';
import {ProfileInternalsBrowserProxy, ProfileInternalsBrowserProxyImpl, ProfileState, ProfileStateElement} from './profile_internals_browser_proxy.js';

const ProfileInternalsAppElementBase = WebUiListenerMixin(PolymerElement);

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
  private profilesList_: ProfileStateElement[];

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'profiles-list-changed',
        (profilesList: ProfileState[]) =>
            this.handleProfilesListChanged_(profilesList));
    this.profileInternalsBrowserProxy_.getProfilesList();
  }

  /**
   * Handler for when the profiles list are updated.
   */
  private handleProfilesListChanged_(profilesList: ProfileState[]) {
    const profilesExpanded = new Map(this.profilesList_.map(
        item => [item.profileState.profilePath, item.expanded]));
    this.profilesList_ = profilesList.map(
        profile => ({
          profileState: profile,
          className: profile.isLoaded ? 'loaded' : 'unloaded',
          expanded: profilesExpanded.get(profile.profilePath) ?? false,
        }));
  }
}

customElements.define(
    ProfileInternalsAppElement.is, ProfileInternalsAppElement);
