// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {AvatarIcon} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './profile_customization_app.css.js';
import {getHtml} from './profile_customization_app.html.js';
import type {ProfileCustomizationBrowserProxy, ProfileInfo} from './profile_customization_browser_proxy.js';
import {ProfileCustomizationBrowserProxyImpl} from './profile_customization_browser_proxy.js';


export interface ProfileCustomizationAppElement {
  $: {
    doneButton: CrButtonElement,
    nameInput: CrInputElement,
    title: HTMLElement,
    viewManager: CrViewManagerElement,
  };
}

const ProfileCustomizationAppElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class ProfileCustomizationAppElement extends
    ProfileCustomizationAppElementBase {
  static get is() {
    return 'profile-customization-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Whether the account is managed (Enterprise). */
      isManaged_: {
        type: Boolean,
      },

      /** Local profile name, editable by user input. */
      profileName_: {
        type: String,
      },

      /** URL for the profile picture. */
      pictureUrl_: {type: String},

      /** Welcome title for the bubble. */
      welcomeTitle_: {type: String},

      /** List of available profile icon URLs and labels. */
      availableIcons_: {type: Array},

      /** The currently selected profile avatar, if any. */
      selectedAvatar_: {type: Object},

      isLocalProfileCreation_: {type: Boolean},
    };
  }

  protected isManaged_: boolean = false;
  protected profileName_: string = '';
  protected pictureUrl_: string;
  protected welcomeTitle_: string;
  protected availableIcons_: AvatarIcon[] = [];
  protected selectedAvatar_: AvatarIcon;
  private confirmedAvatar_: AvatarIcon;
  protected isLocalProfileCreation_: boolean =
      loadTimeData.getBoolean('isLocalProfileCreation');
  private profileCustomizationBrowserProxy_: ProfileCustomizationBrowserProxy =
      ProfileCustomizationBrowserProxyImpl.getInstance();

  override firstUpdated() {
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
  protected onDoneCustomizationClicked_() {
    this.profileCustomizationBrowserProxy_.done(this.profileName_);
  }

  protected isDoneButtonDisabled_(): boolean {
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

  protected shouldShowCancelButton_(): boolean {
    return !this.isLocalProfileCreation_;
  }

  protected onSkipCustomizationClicked_() {
    this.profileCustomizationBrowserProxy_.skip();
  }

  protected onDeleteProfileClicked_() {
    this.profileCustomizationBrowserProxy_.deleteProfile();
  }

  protected onCustomizeAvatarClick_() {
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

  protected onSelectAvatarConfirmClicked_() {
    assert(this.isLocalProfileCreation_);
    this.profileCustomizationBrowserProxy_.setAvatarIcon(
        this.selectedAvatar_.index);
    this.confirmedAvatar_ = this.selectedAvatar_;
    this.closeSelectAvatar_();
  }

  protected onSelectAvatarBackClicked_() {
    assert(this.isLocalProfileCreation_);
    this.closeSelectAvatar_();
    this.selectedAvatar_ = this.confirmedAvatar_;
  }

  private closeSelectAvatar_() {
    this.$.viewManager.switchView('customizeDialog', 'fade-in', 'fade-out');
  }

  protected validateInputOnBlur_() {
    this.$.nameInput.validate();
  }

  protected onProfileNameChanged_(e: CustomEvent<{value: string}>) {
    this.profileName_ = e.detail.value;
  }

  protected onSelectedAvatarChanged_(e: CustomEvent<{value: AvatarIcon}>) {
    this.selectedAvatar_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-customization-app': ProfileCustomizationAppElement;
  }
}

customElements.define(
    ProfileCustomizationAppElement.is, ProfileCustomizationAppElement);
