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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import type {ProfileInfo} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import {ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {AvatarIcon} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './manage_profile.html.js';
import type {ManageProfileBrowserProxy} from './manage_profile_browser_proxy.js';
import {ManageProfileBrowserProxyImpl, ProfileShortcutStatus} from './manage_profile_browser_proxy.js';

const SettingsManageProfileElementBase =
    SettingsViewMixin(RouteObserverMixin(WebUiListenerMixin(PolymerElement)));

export interface SettingsManageProfileElement {
  $: {
    nameInput: CrInputElement,
  };
}

export class SettingsManageProfileElement extends
    SettingsManageProfileElementBase {
  static get is() {
    return 'settings-manage-profile';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The newly selected avatar. Defaults to null, populated only if the user
       * manually changes the avatar selection. The observer ensures that the
       * changes are propagated to the C++.
       */
      profileAvatar_: {
        type: Object,
        observer: 'profileAvatarChanged_',
      },

      /**
       * The current profile name.
       */
      profileName_: String,

      /**
       * True if the current profile has a shortcut.
       */
      hasProfileShortcut_: Boolean,

      /**
       * The available icons for selection.
       */
      availableIcons: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * True if the profile shortcuts feature is enabled.
       */
      isProfileShortcutSettingVisible_: Boolean,

      hasEnterpriseLabel_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('hasEnterpriseLabel');
        },
      },


      /**
       * TODO(dpapad): Move this back to the HTML file when the Polymer2 version
       * of the code is deleted. Because of "\" being a special character in a
       * JS string, can't satisfy both Polymer2 and Polymer3 at the same time
       * from the HTML file.
       */
      pattern_: {
        type: String,
        value: '.*\\S.*',
      },
    };
  }

  declare private profileAvatar_: AvatarIcon;
  declare private profileName_: string;
  declare private hasProfileShortcut_: boolean;
  declare availableIcons: AvatarIcon[];
  declare private isProfileShortcutSettingVisible_: boolean;
  declare private hasEnterpriseLabel_: boolean;
  declare private pattern_: string;
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

  private onProfileInfoChanged_(info: ProfileInfo) {
    this.profileName_ = info.name;
  }

  /**
   * Handler for when the profile name field is changed, then blurred.
   */
  private onNameInputChange_(event: Event) {
    const target = event.target as CrInputElement;
    if (target.invalid) {
      return;
    }

    this.browserProxy_.setProfileName(target.value);
  }

  /**
   * Handler for profile name keydowns.
   */
  private onNameInputKeydown_(event: KeyboardEvent) {
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

  /**
   * Handler for when the profile shortcut toggle is changed.
   */
  private onHasProfileShortcutChange_() {
    if (this.hasProfileShortcut_) {
      this.browserProxy_.addProfileShortcut();
    } else {
      this.browserProxy_.removeProfileShortcut();
    }
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-manage-profile': SettingsManageProfileElement;
  }
}

customElements.define(
    SettingsManageProfileElement.is, SettingsManageProfileElement);
