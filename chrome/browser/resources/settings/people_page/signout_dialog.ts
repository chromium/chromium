// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-signout-dialog' is a dialog that allows the
 * user to turn off sync and sign out of Chromium.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {htmlEscape} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './signout_dialog.css.js';
import {getHtml} from './signout_dialog.html.js';

export interface SettingsSignoutDialogElement {
  $: {
    dialog: CrDialogElement,
    disconnectConfirm: HTMLElement,
  };
}

const SettingsSignoutDialogElementBase = WebUiListenerMixinLit(CrLitElement);

export type SignoutDialogElement = SettingsSignoutDialogElement;

export class SettingsSignoutDialogElement extends
    SettingsSignoutDialogElementBase {
  static get is() {
    return 'settings-signout-dialog';
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
       * The current sync status, supplied by the parent.
       */
      syncStatus: {type: Object},

      /**
       * True if the checkbox to delete the profile has been checked.
       */
      deleteProfile_: {type: Boolean},

      /**
       * True if the profile deletion warning is visible.
       */
      deleteProfileWarningVisible_: {type: Boolean},

      /**
       * The profile deletion warning. The message indicates the number of
       * profile stats that will be deleted if a non-zero count for the profile
       * stats is returned from the browser.
       */
      deleteProfileWarning_: {type: String},
    };
  }

  accessor syncStatus: SyncStatus|null = null;
  protected accessor deleteProfile_: boolean = false;
  protected accessor deleteProfileWarningVisible_: boolean = false;
  protected accessor deleteProfileWarning_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'profile-stats-count-ready',
        (count: number) => this.handleProfileStatsCount_(count));
    // <if expr="not is_chromeos">
    ProfileInfoBrowserProxyImpl.getInstance().getProfileStatsCount();
    // </if>

    this.updateComplete.then(() => {
      if (this.isConnected && !this.$.dialog.open) {
        this.$.dialog.showModal();
      }
    });
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('syncStatus')) {
      this.syncStatusChanged_();
    }
  }

  /**
   * @return true when the user selected 'Confirm'.
   */
  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  /**
   * Handler for when the profile stats count is pushed from the browser.
   */
  private handleProfileStatsCount_(count: number) {
    const username = this.syncStatus?.signedInUsername || '';
    if (count === 0) {
      this.deleteProfileWarning_ = loadTimeData.getStringF(
          'deleteProfileWarningWithoutCounts', username);
    } else if (count === 1) {
      this.deleteProfileWarning_ = loadTimeData.getStringF(
          'deleteProfileWarningWithCountsSingular', username);
    } else {
      this.deleteProfileWarning_ = loadTimeData.getStringF(
          'deleteProfileWarningWithCountsPlural', count, username);
    }
  }

  private syncStatusChanged_() {
    if (!!this.syncStatus &&
        this.syncStatus.signedInState !== SignedInState.SYNCING &&
        this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  // <if expr="not is_chromeos">
  protected getDisconnectExplanationHtml_(): TrustedHTML {
    const domain = this.syncStatus?.domain || '';
    if (domain) {
      return sanitizeInnerHtml(loadTimeData.getStringF(
          'syncDisconnectManagedProfileExplanation',
          `<span>${htmlEscape(domain)}</span>`));
    }
    return sanitizeInnerHtml(
        loadTimeData.getString('syncDisconnectExplanation'));
  }
  // </if>

  // <if expr="is_chromeos">
  protected getDisconnectExplanationHtml_(): TrustedHTML {
    return sanitizeInnerHtml(
        loadTimeData.getString('syncDisconnectExplanation'));
  }
  // </if>

  protected onDeleteProfileCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.deleteProfile_ = e.detail.value;
  }

  protected onDeleteProfileWarningVisibleExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.deleteProfileWarningVisible_ = e.detail.value;
  }

  protected onDisconnectCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onDisconnectConfirmClick_() {
    this.$.dialog.close();
    // <if expr="not is_chromeos">
    SyncBrowserProxyImpl.getInstance().signOut(this.deleteProfile_);
    // </if>
    // <if expr="is_chromeos">
    // Chrome OS users are always signed-in, so just turn off sync.
    SyncBrowserProxyImpl.getInstance().turnOffSync();
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-signout-dialog': SettingsSignoutDialogElement;
  }
}

customElements.define(
    SettingsSignoutDialogElement.is, SettingsSignoutDialogElement);
