// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-signout-dialog' is a dialog that allows the
 * user to turn off sync and sign out of Chromium.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';

import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {WebUIListenerMixin} from '//resources/js/web_ui_listener_mixin.js';
import {microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {ProfileInfoBrowserProxyImpl} from './profile_info_browser_proxy.js';
import {getTemplate} from './signout_dialog.html.js';
import {SyncBrowserProxyImpl, SyncStatus} from './sync_browser_proxy.js';

export interface SettingsSignoutDialogElement {
  $: {
    dialog: CrDialogElement,
    disconnectConfirm: HTMLElement,
  };
}

const SettingsSignoutDialogElementBase = WebUIListenerMixin(PolymerElement);

export class SettingsSignoutDialogElement extends
    SettingsSignoutDialogElementBase {
  static get is() {
    return 'settings-signout-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current sync status, supplied by the parent.
       */
      syncStatus: {
        type: Object,
        observer: 'syncStatusChanged_',
      },

      /**
       * True if the checkbox to delete the profile has been checked.
       */
      deleteProfile_: Boolean,

      /**
       * True if the profile deletion warning is visible.
       */
      deleteProfileWarningVisible_: Boolean,

      /**
       * The profile deletion warning. The message indicates the number of
       * profile stats that will be deleted if a non-zero count for the profile
       * stats is returned from the browser.
       */
      deleteProfileWarning_: String,
    };
  }

  syncStatus: SyncStatus|null;
  private deleteProfile_: boolean;
  private deleteProfileWarningVisible_: boolean;
  private deleteProfileWarning_: string;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'profile-stats-count-ready', this.handleProfileStatsCount_.bind(this));
    // <if expr="not chromeos_ash">
    ProfileInfoBrowserProxyImpl.getInstance().getProfileStatsCount();
    // </if>
    microTask.run(() => {
      this.$.dialog.showModal();
    });
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
    const username = this.syncStatus!.signedInUsername || '';
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

  /**
   * Polymer observer for syncStatus.
   */
  private syncStatusChanged_() {
    if (!this.syncStatus!.signedIn && this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  // <if expr="not chromeos_ash">
  private getDisconnectExplanationHtml_(domain: string): string {
    if (domain) {
      return loadTimeData.getStringF(
          'syncDisconnectManagedProfileExplanation',
          '<span id="managed-by-domain-name">' + domain + '</span>');
    }
    return loadTimeData.getString('syncDisconnectExplanation');
  }
  // </if>

  // <if expr="chromeos_ash">
  private getDisconnectExplanationHtml_(_domain: string): string {
    return loadTimeData.getString('syncDisconnectExplanation');
  }
  // </if>

  private onDisconnectCancel_() {
    this.$.dialog.cancel();
  }

  private onDisconnectConfirm_() {
    this.$.dialog.close();
    // <if expr="not chromeos_ash">
    const deleteProfile = !!this.syncStatus!.domain || this.deleteProfile_;
    SyncBrowserProxyImpl.getInstance().signOut(deleteProfile);
    // </if>
    // <if expr="chromeos_ash">
    // Chrome OS users are always signed-in, so just turn off sync.
    SyncBrowserProxyImpl.getInstance().turnOffSync();
    // </if>
  }

  private isDeleteProfileFooterVisible_(): boolean {
    // <if expr="chromeos_lacros">
    if (!loadTimeData.getBoolean('isSecondaryUser')) {
      // Profile deletion is not allowed for the main profile.
      return false;
    }
    // </if>
    return !this.syncStatus!.domain;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-signout-dialog': SettingsSignoutDialogElement;
  }
}

customElements.define(
    SettingsSignoutDialogElement.is, SettingsSignoutDialogElement);
