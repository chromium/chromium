// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-signout-dialog' is a dialog that allows the
 * user to turn off sync and sign out of Chromium.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../settings_shared.css.js';

import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {SyncBrowserProxyImpl, SyncStatus} from '../../people_page/sync_browser_proxy.js';

import {getTemplate} from './os_signout_dialog.html.js';

export interface OsSettingsSignoutDialogElement {
  $: {
    dialog: CrDialogElement,
    disconnectConfirm: HTMLElement,
  };
}

const OsSettingsSignoutDialogElementBase = WebUiListenerMixin(PolymerElement);

export class OsSettingsSignoutDialogElement extends
    OsSettingsSignoutDialogElementBase {
  static get is() {
    return 'os-settings-signout-dialog';
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

    this.addWebUiListener(
        'profile-stats-count-ready', this.handleProfileStatsCount_.bind(this));
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

  private getDisconnectExplanationHtml_(_domain: string): TrustedHTML {
    return sanitizeInnerHtml(
        loadTimeData.getString('syncDisconnectExplanation'));
  }

  private onDisconnectCancel_() {
    this.$.dialog.cancel();
  }

  private onDisconnectConfirm_() {
    this.$.dialog.close();
    // Chrome OS users are always signed-in, so just turn off sync.
    SyncBrowserProxyImpl.getInstance().turnOffSync();
  }

  private isDeleteProfileFooterVisible_(): boolean {
    return !this.syncStatus!.domain;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-signout-dialog': OsSettingsSignoutDialogElement;
  }
}

customElements.define(
    OsSettingsSignoutDialogElement.is, OsSettingsSignoutDialogElement);
