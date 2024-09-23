// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../icons.html.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../os_settings_icons.html.js';
import '../os_reset_page/os_powerwash_dialog.js';
import './eol_offer_section.js';
import './update_warning_dialog.js';
import '../crostini_page/crostini_settings_card.js';

import {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isCrostiniSupported, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, AboutPageUpdateInfo, BrowserChannel, browserChannelToI18nId, RegulatoryInfo, TpmFirmwareUpdateStatusChangedEvent, UpdateStatus, UpdateStatusChangedEvent} from './about_page_browser_proxy.js';
import {getTemplate} from './os_about_page.html.js';

declare global {
  interface HTMLElementEventMap {
    'target-channel-changed': CustomEvent<BrowserChannel>;
  }
}

export interface OsAboutPageElement {
  $: {
    buttonContainer: HTMLElement,
    checkForUpdatesButton: CrButtonElement,
    extendedUpdatesButton: CrButtonElement,
    productLogo: HTMLImageElement,
    regulatoryInfo: HTMLElement,
    relaunchButton: CrButtonElement,
    updateStatusMessageInner: HTMLDivElement,
  };
}

const OsAboutPageBase = DeepLinkingMixin(
    RouteOriginMixin(I18nMixin(WebUiListenerMixin(PolymerElement))));

export class OsAboutPageElement extends OsAboutPageBase {
  static get is() {
    return 'os-about-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kAboutChromeOs,
        readOnly: true,
      },

      /**
       * Whether the about page is being rendered in dark mode.
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

      currentUpdateStatusEvent_: {
        type: Object,
        value: {
          message: '',
          progress: 0,
          rollback: false,
          powerwash: false,
          status: UpdateStatus.UPDATED,
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
       * The domain of the organization managing the device.
       */
      deviceManager_: {
        type: String,
        value() {
          return loadTimeData.getString('deviceManager');
        },
      },

      hasCheckedForUpdates_: {
        type: Boolean,
        value: false,
      },

      currentChannel_: String,

      targetChannel_: String,

      isLts_: {
        type: Boolean,
        value: false,
      },

      regulatoryInfo_: Object,

      hasEndOfLife_: {
        type: Boolean,
        value: false,
      },

      showEolIncentive_: {
        type: Boolean,
        value: false,
      },

      shouldShowOfferText_: {
        type: Boolean,
        value: false,
      },

      hasDeferredUpdate_: {
        type: Boolean,
        value: false,
      },

      eolMessageWithMonthAndYear_: {
        type: String,
        value: '',
      },

      hasInternetConnection_: {
        type: Boolean,
        value: false,
      },

      firmwareUpdateCount_: {
        type: Number,
        value: 0,
      },

      showCrostiniLicense_: {
        type: Boolean,
        value: false,
      },

      showUpdateStatus_: {
        type: Boolean,
        value: false,
      },

      showButtonContainer_: Boolean,

      showRelaunch_: {
        type: Boolean,
        value: false,
        computed: 'computeShowRelaunch_(currentUpdateStatusEvent_)',
      },

      showCheckUpdates_: {
        type: Boolean,
        computed: 'computeShowCheckUpdates_(' +
            'currentUpdateStatusEvent_, hasCheckedForUpdates_, hasEndOfLife_,' +
            'showExtendedUpdatesOption_)',
      },

      showUpdateWarningDialog_: {
        type: Boolean,
        value: false,
      },

      showTPMFirmwareUpdateLineItem_: {
        type: Boolean,
        value: false,
      },

      showTPMFirmwareUpdateDialog_: Boolean,

      updateInfo_: Object,

      /**
       * Whether the deep link to the check for OS update setting was unable
       * to be shown.
       */
      isPendingOsUpdateDeepLink_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kCheckForOsUpdate,
          Setting.kSeeWhatsNew,
          Setting.kGetHelpWithChromeOs,
          Setting.kReportAnIssue,
          Setting.kTermsOfService,
          Setting.kDiagnostics,
          Setting.kFirmwareUpdates,
        ]),
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              powerWash: 'os-settings:startup',
              releaseNotes: 'os-settings:about-release-notes',
              help: 'os-settings:about-help',
              feedback: 'os-settings:about-feedback',
              diagnostics: 'os-settings:about-diagnostics',
              firmwareUpdates: 'os-settings:about-firmware-updates',
              additionalDetails: 'os-settings:about-additional-details',
            };
          }

          return {
            powerWash: '',
            releaseNotes: '',
            help: '',
            feedback: '',
            diagnostics: '',
            firmwareUpdates: '',
            additionalDetails: '',
          };
        },
      },

      /**
       * Controls whether the extended updates opt-in option is shown.
       */
      showExtendedUpdatesOption_: {
        type: Boolean,
        value: false,
        computed: 'computeShowExtendedUpdatesOption_(' +
            'isExtendedUpdatesOptInEligible_,' +
            'currentUpdateStatusEvent_)',
      },

      /**
       * Whether the device is eligible to opt into extended updates.
       * Value is obtained from the extended updates controller.
       */
      isExtendedUpdatesOptInEligible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether extended updates date has passed.
       * Value is derived from update engine.
       */
      isExtendedUpdatesDatePassed_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether user opt-in is required to receive extended updates.
       * Value is updated from update engine.
       */
      isExtendedUpdatesOptInRequired_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'updateShowUpdateStatus_(hasEndOfLife_, currentUpdateStatusEvent_,' +
          'hasCheckedForUpdates_, showExtendedUpdatesOption_)',
      'updateShowButtonContainer_(showRelaunch_, showCheckUpdates_,' +
          'showExtendedUpdatesOption_)',
      'handleCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
      'updateIsExtendedUpdatesOptInEligible_(' +
          'hasEndOfLife_, isExtendedUpdatesDatePassed_,' +
          'isExtendedUpdatesOptInRequired_)',
    ];
  }

  private isDarkModeActive_: boolean;
  private currentUpdateStatusEvent_: UpdateStatusChangedEvent;
  private isManaged_: boolean;
  private deviceManager_: string;
  private hasCheckedForUpdates_: boolean;
  private currentChannel_: BrowserChannel;
  private targetChannel_: BrowserChannel;
  private isLts_: boolean;
  private regulatoryInfo_: RegulatoryInfo|null;
  private hasEndOfLife_: boolean;
  private showEolIncentive_: boolean;
  private shouldShowOfferText_: boolean;
  private hasDeferredUpdate_: boolean;
  private eolMessageWithMonthAndYear_: string;
  private hasInternetConnection_: boolean;
  private firmwareUpdateCount_: number;
  private rowIcons_: Record<string, string>;
  private showCrostiniLicense_: boolean;
  private showUpdateStatus_: boolean;
  private showButtonContainer_: boolean;
  private showRelaunch_: boolean;
  private showCheckUpdates_: boolean;
  private section_: Section;
  private showUpdateWarningDialog_: boolean;
  private showTPMFirmwareUpdateLineItem_: boolean;
  private showTPMFirmwareUpdateDialog_: boolean;
  private updateInfo_?: AboutPageUpdateInfo;
  private isPendingOsUpdateDeepLink_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private showExtendedUpdatesOption_: boolean;
  private isExtendedUpdatesOptInEligible_: boolean;
  private isExtendedUpdatesDatePassed_: boolean;
  private isExtendedUpdatesOptInRequired_: boolean;

  private aboutBrowserProxy_: AboutPageBrowserProxy;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.ABOUT;

    this.aboutBrowserProxy_ = AboutPageBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.aboutBrowserProxy_.pageReady();

    this.addEventListener(
        'target-channel-changed', (e: CustomEvent<BrowserChannel>) => {
          this.targetChannel_ = e.detail;
        });

    this.aboutBrowserProxy_.getChannelInfo().then(info => {
      this.currentChannel_ = info.currentChannel;
      this.targetChannel_ = info.targetChannel;
      this.isLts_ = info.isLts;
      this.startListening_();
    });

    this.aboutBrowserProxy_.getRegulatoryInfo().then(info => {
      this.regulatoryInfo_ = info;
    });

    this.aboutBrowserProxy_.getEndOfLifeInfo().then(result => {
      this.hasEndOfLife_ = !!result.hasEndOfLife;
      this.eolMessageWithMonthAndYear_ = result.aboutPageEndOfLifeMessage || '';
      this.showEolIncentive_ = !!result.shouldShowEndOfLifeIncentive;
      this.shouldShowOfferText_ = !!result.shouldShowOfferText;
      this.isExtendedUpdatesDatePassed_ = !!result.isExtendedUpdatesDatePassed;
      this.isExtendedUpdatesOptInRequired_ =
          !!result.isExtendedUpdatesOptInRequired;
    });

    this.aboutBrowserProxy_.checkInternetConnection().then(result => {
      this.hasInternetConnection_ = result;
    });

    this.aboutBrowserProxy_.getFirmwareUpdateCount().then(result => {
      this.firmwareUpdateCount_ = result;
    });

    if (Router.getInstance().getQueryParameters().get('checkForUpdate') ===
        'true') {
      this.onCheckUpdatesClick_();
    }

    this.registerExtendedUpdatesObserver_();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(
        routes.ABOUT_DETAILED_BUILD_INFO, '#detailedBuildInfoTrigger');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Only the check for OS update is expected to fail deep link when
        // awaiting the check for update.
        assert(result.pendingSettingId === Setting.kCheckForOsUpdate);
        this.isPendingOsUpdateDeepLink_ = true;
      }
    });
  }

  private startListening_(): void {
    this.addWebUiListener(
        'update-status-changed', this.onUpdateStatusChanged_.bind(this));
    this.aboutBrowserProxy_.refreshUpdateStatus();
    this.addWebUiListener(
        'tpm-firmware-update-status-changed',
        this.onTpmFirmwareUpdateStatusChanged_.bind(this));
    this.aboutBrowserProxy_.refreshTpmFirmwareUpdateStatus();
    this.addWebUiListener(
        'extended-updates-setting-changed',
        this.onExtendedUpdatesSettingChanged_.bind(this));
  }

  private onUpdateStatusChanged_(event: UpdateStatusChangedEvent): void {
    if (event.status === UpdateStatus.CHECKING) {
      this.hasCheckedForUpdates_ = true;
    } else if (event.status === UpdateStatus.NEED_PERMISSION_TO_UPDATE) {
      this.showUpdateWarningDialog_ = true;
      this.updateInfo_ = {version: event.version, size: event.size};
    }
    this.hasDeferredUpdate_ = (event.status === UpdateStatus.DEFERRED);
    this.currentUpdateStatusEvent_ = event;
  }

  private onLearnMoreClick_(event: Event): void {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  }

  private onProductLicenseOtherClicked_(event: CustomEvent<{event: Event}>):
      void {
    // Prevent the default link click behavior
    event.detail.event.preventDefault();

    // Programmatically open license.
    this.aboutBrowserProxy_.openProductLicenseOther();
  }

  private onReleaseNotesClick_(): void {
    this.aboutBrowserProxy_.launchReleaseNotes();
  }

  private onHelpClick_(): void {
    this.aboutBrowserProxy_.openOsHelpPage();
  }

  private onDiagnosticsClick_(): void {
    this.aboutBrowserProxy_.openDiagnostics();
    recordSettingChange(Setting.kDiagnostics);
  }

  private onFirmwareUpdatesClick_(): void {
    this.aboutBrowserProxy_.openFirmwareUpdatesPage();
    recordSettingChange(Setting.kFirmwareUpdates);
  }

  private onRelaunchClick_(): void {
    LifetimeBrowserProxyImpl.getInstance().relaunch();
  }

  private updateShowUpdateStatus_(): void {
    // Do not show the "updated" status or error states from a previous update
    // attempt if we haven't checked yet or the update warning dialog is shown
    // to user.
    if ((this.currentUpdateStatusEvent_.status === UpdateStatus.UPDATED ||
         this.currentUpdateStatusEvent_.status ===
             UpdateStatus.FAILED_DOWNLOAD ||
         this.currentUpdateStatusEvent_.status === UpdateStatus.FAILED_HTTP ||
         this.currentUpdateStatusEvent_.status ===
             UpdateStatus.DISABLED_BY_ADMIN) &&
        (!this.hasCheckedForUpdates_ || this.showUpdateWarningDialog_)) {
      this.showUpdateStatus_ = false;
      return;
    }

    // Do not show "updated" status if the device is end of life or needs to
    // opt into extended updates.
    if (this.hasEndOfLife_ || this.showExtendedUpdatesOption_) {
      this.showUpdateStatus_ = false;
      return;
    }

    this.showUpdateStatus_ =
        this.currentUpdateStatusEvent_.status !== UpdateStatus.DISABLED;
  }

  /**
   * Hide the button container if all buttons are hidden, otherwise the
   * container displays an unwanted border (see separator class).
   */
  private updateShowButtonContainer_(): void {
    this.showButtonContainer_ = this.showRelaunch_ || this.showCheckUpdates_ ||
        this.showExtendedUpdatesOption_;

    // Check if we have yet to focus the check for update button.
    if (!this.isPendingOsUpdateDeepLink_) {
      return;
    }

    this.showDeepLink(Setting.kCheckForOsUpdate).then(result => {
      if (result.deepLinkShown) {
        this.isPendingOsUpdateDeepLink_ = false;
      }
    });
  }

  private computeShowRelaunch_(): boolean {
    return this.checkStatus_(UpdateStatus.NEARLY_UPDATED);
  }

  private shouldShowLearnMoreLink_(): boolean {
    return this.currentUpdateStatusEvent_.status === UpdateStatus.FAILED;
  }

  private shouldShowFirmwareUpdatesBadge_(): boolean {
    return this.firmwareUpdateCount_ > 0;
  }

  private getUpdateStatusMessage_(): TrustedHTML {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.NEED_PERMISSION_TO_UPDATE:
        return this.i18nAdvanced('aboutUpgradeCheckStarted');
      case UpdateStatus.NEARLY_UPDATED:
        if (this.currentChannel_ !== this.targetChannel_) {
          return this.i18nAdvanced('aboutUpgradeSuccessChannelSwitch');
        }
        if (this.currentUpdateStatusEvent_.rollback) {
          return this.i18nAdvanced('aboutRollbackSuccess', {
            substitutions: [this.deviceManager_],
          });
        }
        return this.i18nAdvanced('aboutUpgradeRelaunch');
      case UpdateStatus.UPDATED:
        return this.i18nAdvanced('aboutUpgradeUpToDate');
      case UpdateStatus.UPDATING:
        assert(typeof this.currentUpdateStatusEvent_.progress === 'number');
        const progressPercent = this.currentUpdateStatusEvent_.progress! + '%';

        if (this.currentChannel_ !== this.targetChannel_) {
          return this.i18nAdvanced('aboutUpgradeUpdatingChannelSwitch', {
            substitutions: [
              this.i18nAdvanced(
                      browserChannelToI18nId(this.targetChannel_, this.isLts_))
                  .toString(),
              progressPercent,
            ],
          });
        }
        if (this.currentUpdateStatusEvent_.rollback) {
          return this.i18nAdvanced('aboutRollbackInProgress', {
            substitutions: [this.deviceManager_, progressPercent],
          });
        }
        if (this.currentUpdateStatusEvent_.progress! > 0) {
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
      case UpdateStatus.FAILED_HTTP:
        return this.i18nAdvanced('aboutUpgradeTryAgain');
      case UpdateStatus.FAILED_DOWNLOAD:
        return this.i18nAdvanced('aboutUpgradeDownloadError');
      case UpdateStatus.DISABLED_BY_ADMIN:
        return this.i18nAdvanced('aboutUpgradeAdministrator');
      case UpdateStatus.UPDATE_TO_ROLLBACK_VERSION_DISALLOWED:
        return this.i18nAdvanced('aboutUpdateToRollbackVersionDisallowed');
      case UpdateStatus.DEFERRED:
        return this.i18nAdvanced('aboutUpgradeNotUpToDate');
      default:
        let result = '';
        const message = this.currentUpdateStatusEvent_.message;
        if (message) {
          result += message;
        }
        const connectMessage = this.currentUpdateStatusEvent_.connectionTypes;
        if (connectMessage) {
          result += `<div>${connectMessage}</div>`;
        }
        return sanitizeInnerHtml(result, {tags: ['br', 'pre']});
    }
  }

  private getUpdateStatusIcon_(): string|null {
    // If Chrome OS has reached end of life, display a special icon and
    // ignore UpdateStatus.
    if (this.hasEndOfLife_) {
      return 'os-settings:end-of-life';
    }
    // Show a special icon if extended updates are available.
    // TODO(b/328506053): Finalize icon.
    if (this.showExtendedUpdatesOption_) {
      return 'os-settings:about-update-complete';
    }

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case UpdateStatus.FAILED_DOWNLOAD:
      case UpdateStatus.FAILED_HTTP:
      case UpdateStatus.FAILED:
        return this.isRevampWayfindingEnabled_ ?
            'os-settings:about-update-error' :
            'cr:error-outline';
      case UpdateStatus.UPDATED:
      case UpdateStatus.NEARLY_UPDATED:
        // TODO(crbug.com/40637166): Don't use browser icons here. Fork them.
        return this.isRevampWayfindingEnabled_ ?
            'os-settings:about-update-complete' :
            'settings:check-circle';
      case UpdateStatus.DEFERRED:
      case UpdateStatus.UPDATE_TO_ROLLBACK_VERSION_DISALLOWED:
        return this.isRevampWayfindingEnabled_ ?
            'os-settings:about-update-warning' :
            'cr:warning';
      default:
        return null;
    }
  }

  private getFirmwareUpdatesIcon_(): string {
    if (this.firmwareUpdateCount_ === 0) {
      return '';
    }

    const maxBadgeId = 9;
    // If the number of firmware updates is > 9, then we want to show
    // the 9 badge.
    const updateBadgeId = Math.min(this.firmwareUpdateCount_, maxBadgeId);
    return `os-settings:counter-${updateBadgeId}`;
  }

  private getThrobberSrcIfUpdating_(): string|null {
    if (this.hasEndOfLife_ || this.showExtendedUpdatesOption_) {
      return null;
    }

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return this.isDarkModeActive_ ?
            'chrome://resources/images/throbber_small_dark.svg' :
            'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  }

  private checkStatus_(status: UpdateStatus): boolean {
    return this.currentUpdateStatusEvent_.status === status;
  }

  private onManagementPageClick_(): void {
    window.open('chrome://management');
  }

  private isPowerwash_(): boolean {
    return !!this.currentUpdateStatusEvent_.powerwash;
  }

  private onDetailedBuildInfoClick_(): void {
    Router.getInstance().navigateTo(routes.ABOUT_DETAILED_BUILD_INFO);
  }

  private getRelaunchButtonText_(): string {
    if (this.checkStatus_(UpdateStatus.NEARLY_UPDATED)) {
      return this.i18n(
          this.isPowerwash_() ? 'aboutRelaunchAndPowerwash' : 'aboutRelaunch');
    }
    return '';
  }

  private onCheckUpdatesClick_(): void {
    this.onUpdateStatusChanged_({status: UpdateStatus.CHECKING});
    this.aboutBrowserProxy_.requestUpdate();
    this.$.updateStatusMessageInner.focus();
  }

  private onApplyDeferredUpdateClick_(): void {
    this.aboutBrowserProxy_.applyDeferredUpdate();
    this.$.updateStatusMessageInner.focus();
  }

  private onApplyAndSetAutoUpdateClick_(): void {
    this.aboutBrowserProxy_.setConsumerAutoUpdate(true);
    this.onApplyDeferredUpdateClick_();
  }

  private computeShowCheckUpdates_(): boolean {
    // Disable update button if the device is end of life or needs to opt-in
    // to extended updates.
    if (this.hasEndOfLife_ || this.showExtendedUpdatesOption_) {
      return false;
    }

    // Enable the update button if we are in a stale 'updated' status or
    // update has failed. Disable it otherwise.
    const staleUpdatedStatus =
        !this.hasCheckedForUpdates_ && this.checkStatus_(UpdateStatus.UPDATED);
    return staleUpdatedStatus || this.checkStatus_(UpdateStatus.FAILED) ||
        this.checkStatus_(UpdateStatus.FAILED_HTTP) ||
        this.checkStatus_(UpdateStatus.FAILED_DOWNLOAD) ||
        this.checkStatus_(UpdateStatus.DISABLED_BY_ADMIN) ||
        this.checkStatus_(UpdateStatus.UPDATE_TO_ROLLBACK_VERSION_DISALLOWED);
  }

  /**
   * @param showCrostiniLicense True if Crostini is enabled and
   * Crostini UI is allowed.
   */
  private getAboutProductOsLicense_(showCrostiniLicense: boolean): TrustedHTML {
    return showCrostiniLicense ?
        this.i18nAdvanced('aboutProductOsWithLinuxLicense') :
        this.i18nAdvanced('aboutProductOsLicense');
  }

  /**
   * @param enabled True if Crostini is enabled.
   */
  private handleCrostiniEnabledChanged_(enabled: boolean): void {
    this.showCrostiniLicense_ = enabled && isCrostiniSupported();
  }

  private shouldShowSafetyInfo_(): boolean {
    return loadTimeData.getBoolean('shouldShowSafetyInfo');
  }

  private shouldShowRegulatoryInfo_(): boolean {
    return this.regulatoryInfo_ !== null;
  }

  private shouldShowRegulatoryOrSafetyInfo_(): boolean {
    return this.shouldShowSafetyInfo_() || this.shouldShowRegulatoryInfo_();
  }

  private onUpdateWarningDialogClose_(): void {
    this.showUpdateWarningDialog_ = false;
    // Shows 'check for updates' button in case that the user cancels the
    // dialog and then intends to check for update again.
    this.hasCheckedForUpdates_ = false;
  }

  private onTpmFirmwareUpdateStatusChanged_(
      event: TpmFirmwareUpdateStatusChangedEvent): void {
    this.showTPMFirmwareUpdateLineItem_ = event.updateAvailable;
  }

  private onTpmFirmwareUpdateClick_(): void {
    this.showTPMFirmwareUpdateDialog_ = true;
  }

  private onPowerwashDialogClose_(): void {
    this.showTPMFirmwareUpdateDialog_ = false;
  }

  private onProductLogoClick_(): void {
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
  private onReportIssueClick_(): void {
    this.aboutBrowserProxy_.openFeedbackDialog();
  }

  private getReportIssueLabel_(): string {
    return this.i18n('aboutSendFeedback');
  }
  // </if>

  private shouldShowIcons_(): boolean {
    if (this.hasEndOfLife_) {
      return true;
    }
    return this.showUpdateStatus_;
  }

  private getShowReleaseNotesSublabel_(): string|null {
    return this.isRevampWayfindingEnabled_ ?
        this.i18n('aboutShowReleaseNotesDescription') :
        null;
  }

  private getHelpUsingChromeOsSublabel_(): string|null {
    return this.isRevampWayfindingEnabled_ ?
        this.i18n('aboutGetHelpDescription') :
        null;
  }

  private getReportIssueSublabel_(): string|null {
    return this.isRevampWayfindingEnabled_ ?
        this.i18n('aboutSendFeedbackDescription') :
        null;
  }

  private getDiagnosticsSublabel_(): string|null {
    return this.isRevampWayfindingEnabled_ ?
        this.i18n('aboutDiagnosticseDescription') :
        null;
  }

  private getFirmwareSublabel_(): string|null {
    if (this.isRevampWayfindingEnabled_) {
      return this.firmwareUpdateCount_ > 0 ?
          this.i18n('aboutFirmwareUpdateAvailableDescription') :
          this.i18n('aboutFirmwareUpToDateDescription');
    }
    return null;
  }

  private computeShowExtendedUpdatesOption_(): boolean {
    return this.isExtendedUpdatesOptInEligible_ &&
        this.checkStatus_(UpdateStatus.UPDATED);
  }

  private updateIsExtendedUpdatesOptInEligible_(): void {
    this.aboutBrowserProxy_
        .isExtendedUpdatesOptInEligible(
            this.hasEndOfLife_, this.isExtendedUpdatesDatePassed_,
            this.isExtendedUpdatesOptInRequired_)
        .then(result => {
          this.isExtendedUpdatesOptInEligible_ = result;
        });
  }

  private onExtendedUpdatesSettingChanged_(): void {
    this.updateIsExtendedUpdatesOptInEligible_();
  }

  private onExtendedUpdatesButtonClick_(): void {
    this.aboutBrowserProxy_.openExtendedUpdatesDialog();
  }

  private registerExtendedUpdatesObserver_(): void {
    const extendedUpdatesObserver = new IntersectionObserver(
        (entries: IntersectionObserverEntry[],
         observer: IntersectionObserver) => {
          entries.forEach((entry: IntersectionObserverEntry) => {
            if (entry.isIntersecting) {
              this.aboutBrowserProxy_.recordExtendedUpdatesShown();
              observer.disconnect();
              return;
            }
          });
        });
    extendedUpdatesObserver.observe(this.$.extendedUpdatesButton);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsAboutPageElement.is]: OsAboutPageElement;
  }
}

customElements.define(OsAboutPageElement.is, OsAboutPageElement);
