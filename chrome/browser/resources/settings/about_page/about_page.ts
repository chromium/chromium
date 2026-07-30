// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

import '/shared/settings/prefs/prefs.js';
// <if expr="not is_chromeos">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_page/settings_section.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
// <if expr="_google_chrome">
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
// </if>
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {RelaunchMixinLit, RestartType} from '../relaunch_mixin_lit.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import {getCss} from './about_page.css.js';
import {getHtml} from './about_page.html.js';
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

export interface SettingsAboutPageElement {
  $: {
    productLogo: HTMLImageElement,
    // <if expr="not is_chromeos">
    deprecationWarning: HTMLElement,
    updateStatusMessage: HTMLElement,
    // </if>
  };
}

const SettingsAboutPageElementBase =
    RelaunchMixinLit(PrefServiceObserverMixinLit(
        WebUiListenerMixinLit(I18nMixinLit(CrLitElement))));

export class SettingsAboutPageElement extends SettingsAboutPageElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-about-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentUpdateStatusEvent_: {type: Object},
      isManaged_: {type: Boolean},
      managedByIcon_: {type: String},

      // <if expr="_google_chrome and is_macosx">
      promoteUpdaterStatus_: {type: Object},
      // </if>

      // <if expr="not is_chromeos">
      obsoleteSystemInfo_: {type: Object},
      showUpdateStatus_: {type: Boolean},
      showButtonContainer_: {type: Boolean},
      showRelaunch_: {type: Boolean},
      // </if>

      feedbackAllowedPref_: {type: Object},
    };
  }

  private accessor currentUpdateStatusEvent_: UpdateStatusChangedEvent|null = {
    message: '',
    progress: 0,
    status: UpdateStatus.DISABLED,
  };
  protected accessor isManaged_: boolean = loadTimeData.getBoolean('isManaged');
  protected accessor managedByIcon_: string =
      loadTimeData.getString('managedByIcon');

  // <if expr="_google_chrome and is_macosx">
  protected accessor promoteUpdaterStatus_: PromoteUpdaterStatus = {
    hidden: true,
    disabled: true,
    actionable: false,
    text: '',
  };
  // </if>

  // <if expr="not is_chromeos">
  protected accessor obsoleteSystemInfo_:
      {obsolete: boolean, endOfLine: boolean} = {
        obsolete: loadTimeData.getBoolean('aboutObsoleteNowOrSoon'),
        endOfLine: loadTimeData.getBoolean('aboutObsoleteEndOfTheLine'),
      };
  protected accessor showUpdateStatus_: boolean = false;
  protected accessor showButtonContainer_: boolean = false;
  protected accessor showRelaunch_: boolean = false;
  // </if>

  protected accessor feedbackAllowedPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;

  private aboutBrowserProxy_: AboutPageBrowserProxy =
      AboutPageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.aboutBrowserProxy_.pageReady();

    // <if expr="not is_chromeos">
    this.startListening_();
    // </if>

    // <if expr="_google_chrome">
    this.mirrorPref('feedback_allowed', 'feedbackAllowedPref_');
    // </if>
  }

  // <if expr="not is_chromeos">
  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('obsoleteSystemInfo_') ||
        changedPrivateProperties.has('currentUpdateStatusEvent_')) {
      this.updateShowUpdateStatus_();
    }

    if (changedPrivateProperties.has('currentUpdateStatusEvent_')) {
      this.showRelaunch_ = this.checkStatus_(UpdateStatus.NEARLY_UPDATED);
    }

    if (changedPrivateProperties.has('showRelaunch_')) {
      // Hide the button container if all buttons are hidden, otherwise the
      // container displays an unwanted border (see separator class).
      this.showButtonContainer_ = this.showRelaunch_;
    }
  }
  // </if>

  protected getPromoteUpdaterClass_(): string {
    // <if expr="_google_chrome and is_macosx">
    if (this.promoteUpdaterStatus_.disabled) {
      return 'cr-secondary-text';
    }
    // </if>

    return '';
  }

  // <if expr="not is_chromeos">
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
    this.currentUpdateStatusEvent_ = event;
  }
  // </if>

  // <if expr="_google_chrome and is_macosx">
  private onPromoteUpdaterStatusChanged_(status: PromoteUpdaterStatus) {
    this.promoteUpdaterStatus_ = status;
  }

  /**
   * If #promoteUpdater isn't disabled, trigger update promotion.
   */
  protected onPromoteUpdaterClick_() {
    // This is necessary because #promoteUpdater is not a button, so by default
    // disable doesn't do anything.
    if (this.promoteUpdaterStatus_.disabled) {
      return;
    }
    this.aboutBrowserProxy_.promoteUpdater();
  }
  // </if>

  protected onLearnMoreClick_(event: Event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  }

  protected onHelpClick_() {
    this.aboutBrowserProxy_.openHelpPage();
  }

  protected onRelaunchClick_() {
    this.performRestart(RestartType.RELAUNCH);
  }

  // <if expr="not is_chromeos">
  private updateShowUpdateStatus_() {
    if (this.obsoleteSystemInfo_.endOfLine) {
      this.showUpdateStatus_ = false;
      return;
    }
    this.showUpdateStatus_ =
        this.currentUpdateStatusEvent_!.status !== UpdateStatus.DISABLED;
  }

  protected shouldShowLearnMoreLink_(): boolean {
    return this.currentUpdateStatusEvent_!.status === UpdateStatus.FAILED;
  }

  protected getUpdateStatusMessage_(): TrustedHTML {
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

        if (this.currentUpdateStatusEvent_!.progress > 0) {
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

  protected getUpdateStatusIcon_(): string {
    // If this platform has reached the end of the line, display an error icon
    // and ignore UpdateStatus.
    if (this.obsoleteSystemInfo_.endOfLine) {
      return 'cr:error-filled';
    }

    switch (this.currentUpdateStatusEvent_!.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case UpdateStatus.FAILED:
        return 'cr:error-filled';
      case UpdateStatus.UPDATED:
      case UpdateStatus.NEARLY_UPDATED:
        return 'cr:check-circle';
      default:
        return '';
    }
  }

  protected shouldShowThrobber_(): boolean {
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

  protected onManagementPageClick_() {
    window.location.href = loadTimeData.getString('managementPageUrl');
  }

  protected onProductLogoClick_() {
    this.$.productLogo.animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  }

  // <if expr="_google_chrome">
  protected onReportIssueClick_() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  }

  protected onPrivacyPolicyClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(ABOUT_PAGE_PRIVACY_POLICY_URL);
  }
  // </if>

  // <if expr="not is_chromeos">
  protected shouldShowIcons_(): boolean {
    if (this.obsoleteSystemInfo_.endOfLine) {
      return true;
    }
    return this.showUpdateStatus_;
  }
  // </if>

  // SettingsPlugin implementation
  searchContents(query: string) {
    // settings-about-page is intentionally not included in search.
    return Promise.resolve({
      canceled: false,
      matchCount: 0,
      wasClearSearch: query === '',
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-about-page': SettingsAboutPageElement;
  }
}

customElements.define(SettingsAboutPageElement.is, SettingsAboutPageElement);
