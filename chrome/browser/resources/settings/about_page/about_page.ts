// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

import '../icons.js';
import '../prefs/prefs.js';
// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_page/settings_section.js';
import '../settings_page_css.js';
import '../settings_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {parseHtmlSubset} from 'chrome://resources/js/parse_html_subset.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';

import {getTemplate} from './about_page.html.js';
import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, UpdateStatus, UpdateStatusChangedEvent} from './about_page_browser_proxy.js';
// <if expr="_google_chrome and is_macosx">
import {PromoteUpdaterStatus} from './about_page_browser_proxy.js';

// </if>

const SettingsAboutPageElementBase =
    RelaunchMixin(WebUIListenerMixin(I18nMixin(PolymerElement)));

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
          status: UpdateStatus.DISABLED
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

      // <if expr="_google_chrome and is_macosx">
      promoteUpdaterStatus_: Object,
      // </if>

      // <if expr="not chromeos">
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

  // <if expr="not chromeos">
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

  // <if expr="not chromeos">
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

    // <if expr="not chromeos">
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

  // <if expr="not chromeos">
  private startListening_() {
    this.addWebUIListener(
        'update-status-changed', this.onUpdateStatusChanged_.bind(this));
    // <if expr="_google_chrome and is_macosx">
    this.addWebUIListener(
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
  private onPromoteUpdaterTap_() {
    // This is necessary because #promoteUpdater is not a button, so by default
    // disable doesn't do anything.
    if (this.promoteUpdaterStatus_.disabled) {
      return;
    }
    this.aboutBrowserProxy_.promoteUpdater();
  }
  // </if>

  private onLearnMoreTap_(event: Event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  }

  private onHelpTap_() {
    this.aboutBrowserProxy_.openHelpPage();
  }

  private onRelaunchTap_() {
    this.performRestart(RestartType.RELAUNCH);
  }

  // <if expr="not chromeos">
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

  private getUpdateStatusMessage_(): string {
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
        function formatMessage(msg: string) {
          return (parseHtmlSubset('<b>' + msg + '</b>', ['br', 'pre'])
                      .firstChild as HTMLElement)
              .innerHTML;
        }
        let result = '';
        const message = this.currentUpdateStatusEvent_!.message;
        if (message) {
          result += formatMessage(message);
        }
        const connectMessage = this.currentUpdateStatusEvent_!.connectionTypes;
        if (connectMessage) {
          result += '<div>' + formatMessage(connectMessage) + '</div>';
        }
        return result;
    }
  }

  private getUpdateStatusIcon_(): string|null {
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
        return null;
    }
  }

  private getThrobberSrcIfUpdating_(): string|null {
    if (this.obsoleteSystemInfo_.endOfLine) {
      return null;
    }

    switch (this.currentUpdateStatusEvent_!.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  }
  // </if>

  private checkStatus_(status: UpdateStatus): boolean {
    return this.currentUpdateStatusEvent_!.status === status;
  }

  private onManagementPageTap_() {
    window.location.href = 'chrome://management';
  }

  // <if expr="chromeos">
  private getUpdateOsSettingsLink_(): string {
    // Note: This string contains raw HTML and thus requires i18nAdvanced().
    // Since the i18n template syntax (e.g., $i18n{}) does not include an
    // "advanced" version, it's not possible to inline this link directly in the
    // HTML.
    return this.i18nAdvanced('aboutUpdateOsSettingsLink');
  }
  // </if>

  private onProductLogoTap_() {
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
  private onReportIssueTap_() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  }
  // </if>

  // <if expr="not chromeos">
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
