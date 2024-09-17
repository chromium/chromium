// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

import '../icons.html.js';
import '/shared/settings/prefs/prefs.js';
// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_page/settings_section.js';
import '../settings_page_styles.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
// <if expr="_google_chrome">
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
// </if>
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';

import {getTemplate} from './about_page.html.js';
import type {AboutPageBrowserProxy, UpdateStatusChangedEvent} from './about_page_browser_proxy.js';
import {AboutPageBrowserProxyImpl, UpdateStatus} from './about_page_browser_proxy.js';
// clang-format off
// <if expr="_google_chrome and is_macosx">
import type {PromoteUpdaterStatus} from './about_page_browser_proxy.js';
// </if>
// clang-format on

// <if expr="_google_chrome">
export const ABOUT_PAGE_PRIVACY_POLICY_URL: string =
    'https://policies.google.com/privacy';
// </if>

const SettingsAboutPageElementBase =
    RelaunchMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsAboutPageElement extends SettingsAboutPageElementBase {
  static get is() {
    return 'settings-about-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      currentUpdateStatusEvent_: {
        type: Object,
        value: {
          message: '',
          progress: 0,
          rollback: false,
          status: UpdateStatus.DISABLED,
        },
      },

      /**
       * Whether the browser/ChromeOS is managed by their organization
       * through enterprise policies.
       */
      isManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isManaged');
        },
      },

      /**
       * The name of the icon to display in the management card.
       * Should only be read if isManaged_ is true.
       */
      managedByIcon_: {
        type: String,
        value() {
          return loadTimeData.getString('managedByIcon');
        },
      },

      // <if expr="_google_chrome and is_macosx">
      promoteUpdaterStatus_: Object,
      // </if>

      // <if expr="not chromeos_ash">
      obsoleteSystemInfo_: {
        type: Object,
        value() {
          return {
            obsolete: loadTimeData.getBoolean('aboutObsoleteNowOrSoon'),
            endOfLine: loadTimeData.getBoolean('aboutObsoleteEndOfTheLine'),
          };
        },
      },

      showUpdateStatus_: {
        type: Boolean,
        value: false,
      },

      showButtonContainer_: Boolean,

      showRelaunch_: {
        type: Boolean,
        value: false,
      },
      // </if>
    };
  }

  // <if expr="not chromeos_ash">
  static get observers() {
    return [
      'updateShowUpdateStatus_(' +
          'obsoleteSystemInfo_, currentUpdateStatusEvent_)',
      'updateShowRelaunch_(currentUpdateStatusEvent_)',
      'updateShowButtonContainer_(showRelaunch_)',
    ];
  }
  // </if>

  private currentUpdateStatusEvent_: UpdateStatusChangedEvent|null;
  private isManaged_: boolean;

  // <if expr="_google_chrome and is_macosx">
  private promoteUpdaterStatus_: PromoteUpdaterStatus;
  // </if>

  // <if expr="not chromeos_ash">
  private obsoleteSystemInfo_: {obsolete: boolean, endOfLine: boolean};
  private showUpdateStatus_: boolean;
  private showButtonContainer_: boolean;
  private showRelaunch_: boolean;
  // </if>

  private aboutBrowserProxy_: AboutPageBrowserProxy =
      AboutPageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.aboutBrowserProxy_.pageReady();

    // <if expr="not chromeos_ash">
    this.startListening_();
    // </if>
  }

  private getPromoteUpdaterClass_(): string {
    // <if expr="_google_chrome and is_macosx">
    if (this.promoteUpdaterStatus_.disabled) {
      return 'cr-secondary-text';
    }
    // </if>

    return '';
  }

  // <if expr="not chromeos_ash">
  private startListening_() {
    this.addWebUiListener(
        'update-status-changed', this.onUpdateStatusChanged_.bind(this));
    // <if expr="_google_chrome and is_macosx">
    this.addWebUiListener(
        'promotion-state-changed',
        this.onPromoteUpdaterStatusChanged_.bind(this));
    // </if>
    this.aboutBrowserProxy_.refreshUpdateStatus();
  }

  private onUpdateStatusChanged_(event: UpdateStatusChangedEvent) {
    this.currentUpdateStatusEvent_! = event;
  }
  // </if>

  // <if expr="_google_chrome and is_macosx">
  private onPromoteUpdaterStatusChanged_(status: PromoteUpdaterStatus) {
    this.promoteUpdaterStatus_ = status;
  }

  /**
   * If #promoteUpdater isn't disabled, trigger update promotion.
   */
  private onPromoteUpdaterClick_() {
    // This is necessary because #promoteUpdater is not a button, so by default
    // disable doesn't do anything.
    if (this.promoteUpdaterStatus_.disabled) {
      return;
    }
    this.aboutBrowserProxy_.promoteUpdater();
  }
  // </if>

  private onLearnMoreClick_(event: Event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  }

  private onHelpClick_() {
    this.aboutBrowserProxy_.openHelpPage();
  }

  private onRelaunchClick_() {
    this.performRestart(RestartType.RELAUNCH);
  }

  // <if expr="not chromeos_ash">
  private updateShowUpdateStatus_() {
    if (this.obsoleteSystemInfo_.endOfLine) {
      this.showUpdateStatus_ = false;
      return;
    }
    this.showUpdateStatus_ =
        this.currentUpdateStatusEvent_!.status !== UpdateStatus.DISABLED;
  }

  /**
   * Hide the button container if all buttons are hidden, otherwise the
   * container displays an unwanted border (see separator class).
   */
  private updateShowButtonContainer_() {
    this.showButtonContainer_ = this.showRelaunch_;
  }

  private updateShowRelaunch_() {
    this.showRelaunch_ = this.checkStatus_(UpdateStatus.NEARLY_UPDATED);
  }

  private shouldShowLearnMoreLink_(): boolean {
    return this.currentUpdateStatusEvent_!.status === UpdateStatus.FAILED;
  }

  private getUpdateStatusMessage_(): TrustedHTML {
    switch (this.currentUpdateStatusEvent_!.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.NEED_PERMISSION_TO_UPDATE:
        return this.i18nAdvanced('aboutUpgradeCheckStarted');
      case UpdateStatus.NEARLY_UPDATED:
        return this.i18nAdvanced('aboutUpgradeRelaunch');
      case UpdateStatus.UPDATED:
        return this.i18nAdvanced('aboutUpgradeUpToDate');
      case UpdateStatus.UPDATING:
        assert(typeof this.currentUpdateStatusEvent_!.progress === 'number');
        const progressPercent = this.currentUpdateStatusEvent_!.progress + '%';

        if (this.currentUpdateStatusEvent_!.progress! > 0) {
          // NOTE(dbeam): some platforms (i.e. Mac) always send 0% while
          // updating (they don't support incremental upgrade progress). Though
          // it's certainly quite possible to validly end up here with 0% on
          // platforms that support incremental progress, nobody really likes
          // seeing that they're 0% done with something.
          return this.i18nAdvanced('aboutUpgradeUpdatingPercent', {
            substitutions: [progressPercent],
          });
        }
        return this.i18nAdvanced('aboutUpgradeUpdating');
      default:
        let result = '';
        const message = this.currentUpdateStatusEvent_!.message;
        if (message) {
          result += message;
        }
        const connectMessage = this.currentUpdateStatusEvent_!.connectionTypes;
        if (connectMessage) {
          result += `<div>${connectMessage}</div>`;
        }

        return sanitizeInnerHtml(result, {tags: ['br', 'pre']});
    }
  }

  private getUpdateStatusIcon_(): string {
    // If this platform has reached the end of the line, display an error icon
    // and ignore UpdateStatus.
    if (this.obsoleteSystemInfo_.endOfLine) {
      return 'cr:error';
    }

    switch (this.currentUpdateStatusEvent_!.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case UpdateStatus.FAILED:
        return 'cr:error';
      case UpdateStatus.UPDATED:
      case UpdateStatus.NEARLY_UPDATED:
        return 'settings:check-circle';
      default:
        return '';
    }
  }

  private shouldShowThrobber_(): boolean {
    if (this.obsoleteSystemInfo_.endOfLine) {
      return false;
    }

    switch (this.currentUpdateStatusEvent_!.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return true;
      default:
        return false;
    }
  }
  // </if>

  private checkStatus_(status: UpdateStatus): boolean {
    return this.currentUpdateStatusEvent_!.status === status;
  }

  private onManagementPageClick_() {
    window.location.href = loadTimeData.getString('managementPageUrl');
  }

  private onProductLogoClick_() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  }

  // <if expr="_google_chrome">
  private onReportIssueClick_() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  }

  private onPrivacyPolicyClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(ABOUT_PAGE_PRIVACY_POLICY_URL);
  }
  // </if>

  // <if expr="not chromeos_ash">
  private shouldShowIcons_(): boolean {
    if (this.obsoleteSystemInfo_.endOfLine) {
      return true;
    }
    return this.showUpdateStatus_;
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-about-page': SettingsAboutPageElement;
  }
}

customElements.define(SettingsAboutPageElement.is, SettingsAboutPageElement);
