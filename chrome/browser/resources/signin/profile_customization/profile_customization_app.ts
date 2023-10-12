// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './strings.m.js';
import './signin_shared.css.js';
import './signin_vars.css.js';

import {CustomizeThemesElement} from 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {AvatarIcon} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_customization_app.html.js';
import {ProfileCustomizationBrowserProxy, ProfileCustomizationBrowserProxyImpl, ProfileInfo} from './profile_customization_browser_proxy.js';


export interface ProfileCustomizationAppElement {
  $: {
    doneButton: CrButtonElement,
    nameInput: CrInputElement,
    pickThemeContainer: HTMLElement,
    title: HTMLElement,
    viewManager: CrViewManagerElement,
  };
}

const ProfileCustomizationAppElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class ProfileCustomizationAppElement extends
    ProfileCustomizationAppElementBase {
  static get is() {
    return 'profile-customization-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether the account is managed (Enterprise). */
      isManaged_: {
        type: Boolean,
        value: false,
      },

      /** Local profile name, editable by user input. */
      profileName_: {
        type: String,
        value: '',
      },

      /** URL for the profile picture. */
      pictureUrl_: String,

      /** Welcome title for the bubble. */
      welcomeTitle_: String,

      /** List of available profile icon URLs and labels. */
      availableIcons_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** The currently selected profile avatar, if any. */
      selectedAvatar_: Object,

      isLocalProfileCreation_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isLocalProfileCreation'),
      },

      isChromeRefresh2023_: {
        type: Boolean,
        value: () =>
            document.documentElement.hasAttribute('chrome-refresh-2023'),
      },
    };
  }

  private isManaged_: boolean;
  private profileName_: string;
  private pictureUrl_: string;
  private welcomeTitle_: string;
  private availableIcons_: AvatarIcon[];
  private selectedAvatar_: AvatarIcon;
  private confirmedAvatar_: AvatarIcon;
  private isLocalProfileCreation_: boolean;
  private isChromeRefresh2023_: boolean;
  private profileCustomizationBrowserProxy_: ProfileCustomizationBrowserProxy =
      ProfileCustomizationBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    // profileName_ is only set now, because it triggers a validation of the
    // input which crashes if it's done too early.
    if (!this.isLocalProfileCreation_) {
      this.profileName_ = loadTimeData.getString('profileName');
    }
    this.addWebUiListener(
        'on-profile-info-changed',
        (info: ProfileInfo) => this.setProfileInfo_(info));
    this.addWebUiListener(
        'on-available-icons-changed',
        (icons: AvatarIcon[]) => this.setAvailableIcons_(icons));
    this.profileCustomizationBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
    if (this.isLocalProfileCreation_) {
      this.profileCustomizationBrowserProxy_.getAvailableIcons().then(
          icons => this.setAvailableIcons_(icons));
    }
  }

  /**
   * Called when the Done button is clicked. Sends the profile name back to
   * native.
   */
  private onDoneCustomizationClicked_() {
    if (!this.isChromeRefresh2023_) {
      const themeSelector = this.$.pickThemeContainer.querySelector(
                                '#themeSelector')! as CustomizeThemesElement;
      themeSelector.confirmThemeChanges();
    }
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
    this.welcomeTitle_ = this.isLocalProfileCreation_ ?
        this.i18n('localProfileCreationTitle') :
        this.i18n('profileCustomizationTitle');
  }

  private shouldShowCancelButton_(): boolean {
    return !this.isLocalProfileCreation_;
  }

  private onSkipCustomizationClicked_() {
    this.profileCustomizationBrowserProxy_.skip();
  }

  private onDeleteProfileClicked_() {
    // Unsaved theme color changes cause an error in `ProfileCustomizationUI`
    // destructor when deleting the profile.
    if (!this.isChromeRefresh2023_) {
      const themeSelector = this.$.pickThemeContainer.querySelector(
                                '#themeSelector')! as CustomizeThemesElement;
      themeSelector.confirmThemeChanges();
    }
    this.profileCustomizationBrowserProxy_.deleteProfile();
  }

  private onCustomizeAvatarClick_() {
    assert(this.isLocalProfileCreation_);
    this.$.viewManager.switchView('selectAvatarDialog', 'fade-in', 'fade-out');
  }

  private setAvailableIcons_(icons: AvatarIcon[]) {
    // If there is no selectedAvatar_ yet, get it from the icons list.
    // Setting all the icons in availableIcons_ as not selected so the only
    // source of truth for the currently selected icon is selectedAvatar_ and
    // there is only one icon marked as selected.
    icons.forEach((icon, index) => {
      if (icon.selected) {
        icons[index].selected = false;
        this.confirmedAvatar_ = icons[index];
        if (!this.selectedAvatar_) {
          this.selectedAvatar_ = icons[index];
        }
      }
    });
    this.availableIcons_ = icons;
  }

  private onSelectAvatarConfirmClicked_() {
    assert(this.isLocalProfileCreation_);
    this.profileCustomizationBrowserProxy_.setAvatarIcon(
        this.selectedAvatar_.index);
    this.confirmedAvatar_ = this.selectedAvatar_;
    this.closeSelectAvatar_();
  }

  private onSelectAvatarBackClicked_() {
    assert(this.isLocalProfileCreation_);
    this.closeSelectAvatar_();
    this.selectedAvatar_ = this.confirmedAvatar_;
  }

  private closeSelectAvatar_() {
    this.$.viewManager.switchView('customizeDialog', 'fade-in', 'fade-out');
  }

  private validateInputOnBlur_() {
    this.$.nameInput.validate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-customization-app': ProfileCustomizationAppElement;
  }
}

customElements.define(
    ProfileCustomizationAppElement.is, ProfileCustomizationAppElement);
