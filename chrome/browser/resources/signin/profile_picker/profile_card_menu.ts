// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import './profile_picker_shared.css.js';
import './icons.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
// <if expr="chromeos_lacros">
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// </if>

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// <if expr="chromeos_lacros">
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// </if>

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {getTemplate} from './profile_card_menu.html.js';

export type Statistics = {
  BrowsingHistory: number,
  Passwords: number,
  Bookmarks: number,
  Autofill: number,
};

/**
 * This is the data structure sent back and forth between C++ and JS.
 */
export type StatisticsResult = {
  profilePath: string,
  statistics: Statistics,
};

/**
 * Profile statistics data types.
 */
enum ProfileStatistics {
  BROWSING_HISTORY = 'BrowsingHistory',
  PASSWORDS = 'Passwords',
  BOOKMARKS = 'Bookmarks',
  AUTOFILL = 'Autofill',
}

export interface ProfileCardMenuElement {
  $: {
    actionMenu: CrActionMenuElement,
    moreActionsButton: HTMLElement,
    removeConfirmationDialog: CrDialogElement,
    removePrimaryLacrosProfileDialog: CrDialogElement,
  };
}

const ProfileCardMenuElementBase =
    WebUIListenerMixin(I18nMixin(PolymerElement));

export class ProfileCardMenuElement extends ProfileCardMenuElementBase {
  static get is() {
    return 'profile-card-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profileState: Object,

      /**
       * Results of profile statistics, keyed by the suffix of the corresponding
       * data type, as reported by the C++ side.
       */
      statistics_: {
        type: Object,
        // Will be filled as results are reported.
        value() {
          return {};
        },
      },

      /**
       * List of selected data types.
       */
      profileStatistics_: {
        type: Array,
        value: [
          ProfileStatistics.BROWSING_HISTORY,
          ProfileStatistics.PASSWORDS,
          ProfileStatistics.BOOKMARKS,
          ProfileStatistics.AUTOFILL,
        ],
      },

      removeWarningText_: {
        type: String,
        // <if expr="chromeos_lacros">
        value() {
          return loadTimeData.getString('removeWarningProfileLacros');
        },
        // </if>
        // <if expr="not chromeos_lacros">
        computed: 'computeRemoveWarningText_(profileState)',
        // </if>
      },

      removeWarningTitle_: {
        type: String,
        computed: 'computeRemoveWarningTitle_(profileState)',
      },

      // <if expr="chromeos_lacros">
      removePrimaryLacrosProfileWarning_: {
        type: String,
        computed: 'computeRemovePrimaryLacrosProfileWarning_(profileState)',
      },
      // </if>

    };
  }

  profileState: ProfileState;
  private statistics_: {[key: string]: number};
  private profileStatistics_: ProfileStatistics[];
  private removeWarningText_: string;
  private removeWarningTitle_: string;
  // <if expr="chromeos_lacros">
  private removePrimaryLacrosProfileWarning_: string;
  // </if>
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUIListener(
        'profiles-list-changed', () => this.handleProfilesUpdated_());
    this.addWebUIListener(
        'profile-removed', this.handleProfileRemoved_.bind(this));
    this.addWebUIListener(
        'profile-statistics-received',
        this.handleProfileStatsReceived_.bind(this));
  }

  override ready() {
    super.ready();
    // <if expr="chromeos_lacros">
    afterNextRender(this, () => {
      this.shadowRoot!.querySelector('#removeWarningHeader a')!
          .addEventListener('click', () => this.onAccountSettingsClicked_());
    });
    // </if>
  }

  // <if expr="not chromeos_lacros">
  private computeRemoveWarningText_(): string {
    return this.i18n(
        this.profileState.isSyncing ? 'removeWarningSignedInProfile' :
                                      'removeWarningLocalProfile');
  }
  // </if>

  private computeRemoveWarningTitle_(): string {
    return this.i18n(
        this.profileState.isSyncing ? 'removeWarningSignedInProfileTitle' :
                                      'removeWarningLocalProfileTitle');
  }

  // <if expr="chromeos_lacros">
  private computeRemovePrimaryLacrosProfileWarning_(): string {
    return this.i18n(
        'lacrosPrimaryProfileDeletionWarning', this.profileState.userName,
        loadTimeData.getString('deviceType'));
  }
  // </if>

  private onMoreActionsButtonClicked_(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    this.$.actionMenu.showAt(this.$.moreActionsButton);
    chrome.metricsPrivate.recordUserAction(
        'ProfilePicker_ThreeDottedMenuClicked');
  }

  private onRemoveButtonClicked_(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    this.manageProfilesBrowserProxy_.getProfileStatistics(
        this.profileState.profilePath);
    this.$.actionMenu.close();
    // <if expr="chromeos_lacros">
    if (this.profileState.isPrimaryLacrosProfile) {
      this.$.removePrimaryLacrosProfileDialog.showModal();
    } else {
      this.$.removeConfirmationDialog.showModal();
    }
    // </if>
    // <if expr="not chromeos_lacros">
    this.$.removeConfirmationDialog.showModal();
    // </if>
    chrome.metricsPrivate.recordUserAction('ProfilePicker_RemoveOptionClicked');
  }

  private handleProfileStatsReceived_(result: StatisticsResult) {
    if (result.profilePath !== this.profileState.profilePath) {
      return;
    }
    this.statistics_ = result.statistics;
  }

  private getProfileStatisticText_(dataType: ProfileStatistics): string {
    switch (dataType) {
      case ProfileStatistics.BROWSING_HISTORY:
        return this.i18n('removeWarningHistory');
      case ProfileStatistics.PASSWORDS:
        return this.i18n('removeWarningPasswords');
      case ProfileStatistics.BOOKMARKS:
        return this.i18n('removeWarningBookmarks');
      case ProfileStatistics.AUTOFILL:
        return this.i18n('removeWarningAutofill');
      default:
        assertNotReached();
        return '';
    }
  }

  private getProfileStatisticCount_(dataType: string): string {
    const count = this.statistics_[dataType];
    return (count === undefined) ? this.i18n('removeWarningCalculating') :
                                   count.toString();
  }

  private onRemoveConfirmationClicked_(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    this.manageProfilesBrowserProxy_.removeProfile(
        this.profileState.profilePath);
  }

  private onRemoveCancelClicked_() {
    this.$.removeConfirmationDialog.cancel();
  }

  // <if expr="chromeos_lacros">
  private onRemovePrimaryLacrosProfileCancelClicked_() {
    this.$.removePrimaryLacrosProfileDialog.cancel();
  }
  // </if>

  /**
   * Ensure any menu is closed on profile list updated.
   */
  private handleProfilesUpdated_() {
    this.$.actionMenu.close();
  }

  /**
   * Closes the remove confirmation dialog when the profile is removed.
   */
  private handleProfileRemoved_(profilePath: string) {
    this.handleProfilesUpdated_();
    if (this.profileState.profilePath === profilePath) {
      this.$.removeConfirmationDialog.close();
    }
  }

  private onCustomizeButtonClicked_() {
    this.manageProfilesBrowserProxy_.openManageProfileSettingsSubPage(
        this.profileState.profilePath);
    this.$.actionMenu.close();
  }

  // <if expr="chromeos_lacros">
  private onAccountSettingsClicked_() {
    this.manageProfilesBrowserProxy_.openAshAccountSettingsPage();
    this.$.removeConfirmationDialog.close();
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-card-menu': ProfileCardMenuElement;
  }
}

customElements.define(ProfileCardMenuElement.is, ProfileCardMenuElement);
