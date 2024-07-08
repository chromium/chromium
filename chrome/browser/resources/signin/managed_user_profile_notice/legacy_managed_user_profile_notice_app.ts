// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './icons.html.js';
import './strings.m.js';
import './signin_shared.css.js';
import './signin_vars.css.js';
import './tangible_sync_style_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './legacy_managed_user_profile_notice_app.html.js';
import type {ManagedUserProfileInfo, ManagedUserProfileNoticeBrowserProxy} from './managed_user_profile_notice_browser_proxy.js';
import {ManagedUserProfileNoticeBrowserProxyImpl, State} from './managed_user_profile_notice_browser_proxy.js';

document.addEventListener('DOMContentLoaded', () => {
  const managedUserProfileNoticeBrowserProxyImpl =
      ManagedUserProfileNoticeBrowserProxyImpl.getInstance();
  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // page even when the page zoom in Chrome is different than 100%.
  managedUserProfileNoticeBrowserProxyImpl.initializedWithSize(
      document.body.offsetHeight);
  // The web dialog size has been initialized, so reset the body width to
  // auto. This makes sure that the body only takes up the viewable width,
  // e.g. when there is a scrollbar.
  document.body.style.width = 'auto';
});

const LegacyManagedUserProfileNoticeAppElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class LegacyManagedUserProfileNoticeAppElement extends
    LegacyManagedUserProfileNoticeAppElementBase {
  static get is() {
    return 'legacy-managed-user-profile-notice-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether the account is managed */
      showEnterpriseBadge_: {
        type: Boolean,
        value: false,
      },

      /** URL for the profile picture */
      pictureUrl_: String,

      /** The title and subtitle of the screen */
      title_: String,
      subtitle_: String,

      /** The detailed info about enterprise management */
      enterpriseInfo_: String,

      /**
       * Whether this page is being shown as a dialog.
       *
       * Reflected as an attribute to allow configuring variables and styles at
       * the element host level.
       */
      isModalDialog_: {
        type: Boolean,
        reflectToAttribute: true,
        value() {
          return loadTimeData.getBoolean('isModalDialog');
        },
      },

      showLinkDataCheckbox_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showLinkDataCheckbox');
        },
      },

      useLegacyUi_: {
        type: Boolean,
        reflectToAttribute: true,
        value() {
          return !loadTimeData.getBoolean('useUpdatedUi');
        },
      },

      /** The label for the button to proceed with the flow */
      proceedLabel_: String,

      /** Whether to show the cancel button on the screen */
      showCancelButton_: {
        type: Boolean,
        value: true,
      },

      disableProceedButton_: {
        type: Boolean,
        value: false,
      },

      linkData_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: 'linkDataChanged_',
      },
    };
  }

  private showEnterpriseBadge_: boolean;
  private pictureUrl_: string;
  private title_: string;
  private subtitle_: string;
  private enterpriseInfo_: string;
  private isModalDialog_: boolean;
  private proceedLabel_: string;
  private disableProceedButton_: boolean;
  private linkData_: boolean;
  private showCancelButton_: boolean;
  private defaultProceedLabel_: string;
  private managedUserProfileNoticeBrowserProxy_:
      ManagedUserProfileNoticeBrowserProxy =
          ManagedUserProfileNoticeBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUiListener(
        'on-profile-info-changed',
        (info: ManagedUserProfileInfo) => this.setProfileInfo_(info));
    this.managedUserProfileNoticeBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
  }

  private linkDataChanged_(linkData: boolean) {
    this.proceedLabel_ =
        linkData ? this.i18n('continueLabel') : this.defaultProceedLabel_;
  }

  /** Called when the proceed button is clicked. */
  private onProceed_() {
    this.disableProceedButton_ = true;
    this.managedUserProfileNoticeBrowserProxy_.proceed(
        State.SUCCESS, this.linkData_);
  }

  /** Called when the cancel button is clicked. */
  private onCancel_() {
    this.managedUserProfileNoticeBrowserProxy_.cancel();
  }

  private setProfileInfo_(info: ManagedUserProfileInfo) {
    this.pictureUrl_ = info.pictureUrl;
    this.showEnterpriseBadge_ = info.showEnterpriseBadge;
    this.title_ = info.title;
    this.subtitle_ = info.subtitle;
    this.enterpriseInfo_ = info.enterpriseInfo;
    this.defaultProceedLabel_ = info.proceedLabel;
    this.proceedLabel_ = this.defaultProceedLabel_;
    this.showCancelButton_ = info.showCancelButton;
    this.linkData_ = info.checkLinkDataCheckboxByDefault;
  }

  /**
   * Returns either "dialog" or an empty string.
   *
   * The returned value is intended to be added as a class on the root tags of
   * the element. Some styles from `tangible_sync_style_shared.css` rely on the
   * presence of this "dialog" class.
   */
  private getMaybeDialogClass_() {
    return this.isModalDialog_ ? 'dialog' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'legacy-managed-user-profile-notice-app':
        LegacyManagedUserProfileNoticeAppElement;
  }
}

customElements.define(
    LegacyManagedUserProfileNoticeAppElement.is,
    LegacyManagedUserProfileNoticeAppElement);
