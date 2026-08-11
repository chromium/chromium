// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-profile' is the settings subpage containing controls to
 * edit a profile's name, icon, and desktop shortcut.
 */
import 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../settings_page/settings_subpage.js';

import type {ProfileInfo} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import {ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {AvatarIcon} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './manage_profile.css.js';
import {getHtml} from './manage_profile.html.js';
import type {ManageProfileBrowserProxy} from './manage_profile_browser_proxy.js';
import {ManageProfileBrowserProxyImpl, ProfileShortcutStatus} from './manage_profile_browser_proxy.js';

const SettingsManageProfileElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(CrLitElement));

export interface SettingsManageProfileElement {
  $: {
    nameInput: CrInputElement,
  };
}

export type ManageProfileElement = SettingsManageProfileElement;

export class SettingsManageProfileElement extends
    SettingsManageProfileElementBase {
  static get is() {
    return 'settings-manage-profile';
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
       * The newly selected avatar. Defaults to null, populated only if the user
       * manually changes the avatar selection. The observer ensures that the
       * changes are propagated to the C++.
       */
      profileAvatar_: {type: Object},

      /**
       * The current profile name.
       */
      profileName_: {type: String},

      /**
       * True if the current profile has a shortcut.
       */
      hasProfileShortcut_: {type: Boolean},

      /**
       * The available icons for selection.
       */
      availableIcons: {type: Array},

      /**
       * True if the profile shortcuts feature is enabled.
       */
      isProfileShortcutSettingVisible_: {type: Boolean},

      hasEnterpriseLabel_: {type: Boolean},

      /**
       * TODO(dpapad): Move this back to the HTML file when the Polymer2 version
       * of the code is deleted. Because of "\" being a special character in a
       * JS string, can't satisfy both Polymer2 and Polymer3 at the same time
       * from the HTML file.
       */
      pattern_: {type: String},
    };
  }

  protected accessor profileAvatar_: AvatarIcon|null = null;
  protected accessor profileName_: string = '';
  protected accessor hasProfileShortcut_: boolean = false;
  accessor availableIcons: AvatarIcon[] = [];
  protected accessor isProfileShortcutSettingVisible_: boolean = false;
  protected accessor hasEnterpriseLabel_: boolean =
      loadTimeData.getBoolean('hasEnterpriseLabel');
  protected accessor pattern_: string = '.*\\S.*';
  private browserProxy_: ManageProfileBrowserProxy =
      ManageProfileBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    const setIcons = (icons: AvatarIcon[]) => {
      this.availableIcons = icons;
    };

    this.addWebUiListener('available-icons-changed', setIcons);
    this.browserProxy_.getAvailableIcons().then(setIcons);

    ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
        this.onProfileInfoChanged_.bind(this));
    this.addWebUiListener(
        'profile-info-changed', this.onProfileInfoChanged_.bind(this));
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    if (Router.getInstance().getCurrentRoute() === routes.MANAGE_PROFILE) {
      if (this.profileName_) {
        this.$.nameInput.value = this.profileName_;
      }
      if (loadTimeData.getBoolean('profileShortcutsEnabled')) {
        this.browserProxy_.getProfileShortcutStatus().then(status => {
          if (status ===
              ProfileShortcutStatus.PROFILE_SHORTCUT_SETTING_HIDDEN) {
            this.isProfileShortcutSettingVisible_ = false;
            return;
          }

          this.isProfileShortcutSettingVisible_ = true;
          this.hasProfileShortcut_ =
              status === ProfileShortcutStatus.PROFILE_SHORTCUT_FOUND;
        });
      }
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('profileAvatar_')) {
      this.profileAvatarChanged_();
    }
  }

  private onProfileInfoChanged_(info: ProfileInfo) {
    this.profileName_ = info.name;
  }

  /**
   * Handler for when the profile name field is changed, then blurred.
   */
  protected onNameInputChange_(event: Event) {
    const target = event.target as CrInputElement;
    if (target.invalid) {
      return;
    }

    this.browserProxy_.setProfileName(target.value);
  }

  /**
   * Handler for profile name keydowns.
   */
  protected onNameInputKeydown_(event: KeyboardEvent) {
    if (event.key === 'Escape') {
      const target = event.target as CrInputElement;
      target.value = this.profileName_;
      target.blur();
    }
  }

  /**
   * Handler for when the profile avatar is changed by the user.
   */
  private profileAvatarChanged_() {
    if (this.profileAvatar_ === null) {
      return;
    }

    if (this.profileAvatar_.isGaiaAvatar) {
      this.browserProxy_.setProfileIconToGaiaAvatar();
    } else {
      this.browserProxy_.setProfileIconToDefaultAvatar(
          this.profileAvatar_.index);
    }
  }

  protected onSelectedAvatarChanged_(e: CustomEvent<{value: AvatarIcon}>) {
    this.profileAvatar_ = e.detail.value;
  }

  /**
   * Handler for when the profile shortcut toggle is changed.
   */
  protected onHasProfileShortcutChange_(event: CustomEvent<boolean>) {
    this.hasProfileShortcut_ = event.detail;
    if (this.hasProfileShortcut_) {
      this.browserProxy_.addProfileShortcut();
    } else {
      this.browserProxy_.removeProfileShortcut();
    }
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-manage-profile': SettingsManageProfileElement;
  }
}

customElements.define(
    SettingsManageProfileElement.is, SettingsManageProfileElement);
