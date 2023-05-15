// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import './profile_picker_shared.css.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
// clang-format off
// <if expr="chromeos_lacros">
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// </if>

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on



import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {getTemplate} from './profile_card_menu.html.js';

export interface Statistics {
  BrowsingHistory: number;
  Passwords: number;
  Bookmarks: number;
  Autofill: number;
}

/**
 * This is the data structure sent back and forth between C++ and JS.
 */
export interface StatisticsResult {
  profilePath: string;
  statistics: Statistics;
}

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
    WebUiListenerMixin(I18nMixin(PolymerElement));

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

      moreActionsButtonAriaLabel_: {
        type: String,
        computed: 'computeMoreActionsButtonAriaLabel_(profileState)',
      },

      removeWarningText_: {
        type: String,
        // <if expr="chromeos_lacros">
        value() {
          return sanitizeInnerHtml(
              loadTimeData.getString('removeWarningProfileLacros'),
              {attrs: ['is']});
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
  private statistics_: Statistics;
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
    this.addWebUiListener(
        'profiles-list-changed', () => this.handleProfilesUpdated_());
    this.addWebUiListener(
        'profile-removed', this.handleProfileRemoved_.bind(this));
    this.addWebUiListener(
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

  private computeMoreActionsButtonAriaLabel_(): string {
    return this.i18n(
        'profileMenuAriaLabel', this.profileState.localProfileName);
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
    }
  }

  private getProfileStatisticCount_(dataType: keyof Statistics): string {
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
    this.manageProfilesBrowserProxy_.closeProfileStatistics();
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
