// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './icons.html.js';
import './strings.m.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './legacy_managed_user_profile_notice_app.css.js';
import {getHtml} from './legacy_managed_user_profile_notice_app.html.js';
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
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class LegacyManagedUserProfileNoticeAppElement extends
    LegacyManagedUserProfileNoticeAppElementBase {
  static get is() {
    return 'legacy-managed-user-profile-notice-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Whether the account is managed */
      showEnterpriseBadge_: {type: Boolean},

      /** URL for the profile picture */
      pictureUrl_: {type: String},

      /** The title and subtitle of the screen */
      title_: {type: String},
      subtitle_: {type: String},

      /** The detailed info about enterprise management */
      enterpriseInfo_: {type: String},

      /**
       * Whether this page is being shown as a dialog.
       *
       * Reflected as an attribute to allow configuring variables and styles at
       * the element host level.
       */
      isModalDialog_: {
        type: Boolean,
        reflect: true,
      },

      showLinkDataCheckbox_: {type: Boolean},

      useLegacyUi_: {
        type: Boolean,
        reflect: true,
      },

      /** The label for the button to proceed with the flow */
      proceedLabel_: {type: String},

      disableProceedButton_: {type: Boolean},

      linkData_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected showEnterpriseBadge_: boolean = false;
  protected pictureUrl_: string;
  protected title_: string;
  protected subtitle_: string;
  protected enterpriseInfo_: string;
  protected isModalDialog_: boolean = loadTimeData.getBoolean('isModalDialog');
  protected showLinkDataCheckbox_: boolean =
      loadTimeData.getBoolean('showLinkDataCheckbox');
  protected useLegacyUi_: boolean = !loadTimeData.getBoolean('useUpdatedUi');
  protected proceedLabel_: string;
  protected disableProceedButton_: boolean = false;
  protected linkData_: boolean = false;
  private defaultProceedLabel_: string;
  private managedUserProfileNoticeBrowserProxy_:
      ManagedUserProfileNoticeBrowserProxy =
          ManagedUserProfileNoticeBrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('linkData_')) {
      this.proceedLabel_ = this.linkData_ ? this.i18n('continueLabel') :
                                            this.defaultProceedLabel_;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addWebUiListener(
        'on-profile-info-changed',
        (info: ManagedUserProfileInfo) => this.setProfileInfo_(info));
    this.managedUserProfileNoticeBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
  }

  /** Called when the proceed button is clicked. */
  protected onProceed_() {
    this.disableProceedButton_ = true;
    this.managedUserProfileNoticeBrowserProxy_.proceed(
        State.SUCCESS, this.linkData_);
  }

  /** Called when the cancel button is clicked. */
  protected onCancel_() {
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
    this.linkData_ = info.checkLinkDataCheckboxByDefault;
  }

  /**
   * Returns either "dialog" or an empty string.
   *
   * The returned value is intended to be added as a class on the root tags of
   * the element. Some styles from `tangible_sync_style_shared.css` rely on the
   * presence of this "dialog" class.
   */
  protected getMaybeDialogClass_() {
    return this.isModalDialog_ ? 'dialog' : '';
  }

  protected onLinkDataChanged_(e: CustomEvent<{value: boolean}>) {
    this.linkData_ = e.detail.value;
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
