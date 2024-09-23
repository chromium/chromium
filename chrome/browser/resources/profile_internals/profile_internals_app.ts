// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './strings.m.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './profile_internals_app.css.js';
import {getHtml} from './profile_internals_app.html.js';
import type {ProfileInternalsBrowserProxy, ProfileState, ProfileStateElement} from './profile_internals_browser_proxy.js';
import {ProfileInternalsBrowserProxyImpl} from './profile_internals_browser_proxy.js';

const ProfileInternalsAppElementBase = WebUiListenerMixinLit(CrLitElement);

export class ProfileInternalsAppElement extends ProfileInternalsAppElementBase {
  static get is() {
    return 'profile-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Profiles list supplied by ProfileInternalsBrowserProxy.
       */
      profilesList_: {type: Array},
    };
  }

  private profileInternalsBrowserProxy_: ProfileInternalsBrowserProxy =
      ProfileInternalsBrowserProxyImpl.getInstance();

  protected profilesList_: ProfileStateElement[] = [];

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

  protected onExpandedChanged_(event: CustomEvent<{value: boolean}>) {
    const currentTarget = event.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const profile = this.profilesList_[index];
    assert(profile);
    profile.expanded = event.detail.value;
    this.requestUpdate();
  }
}

customElements.define(
    ProfileInternalsAppElement.is, ProfileInternalsAppElement);
