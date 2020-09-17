// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

Polymer({
  is: 'os-settings-about-page',

  behaviors: [
    DeepLinkingBehavior,
    WebUIListenerBehavior,
    settings.MainPageBehavior,
    settings.RouteObserverBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @private {?UpdateStatusChangedEvent} */
    currentUpdateStatusEvent_: {
      type: Object,
      value: {
        message: '',
        progress: 0,
        rollback: false,
        powerwash: false,
        status: UpdateStatus.DISABLED
      },
    },

    /**
     * Whether the browser/ChromeOS is managed by their organization
     * through enterprise policies.
     * @private
     */
    isManaged_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isManaged');
      },
    },

    /** @private */
    hasCheckedForUpdates_: {
      type: Boolean,
      value: false,
    },

    /** @private {!BrowserChannel} */
    currentChannel_: String,

    /** @private {!BrowserChannel} */
    targetChannel_: String,

    /** @private */
    isLts_: {
      type: Boolean,
      value: false,
    },

    /** @private {?RegulatoryInfo} */
    regulatoryInfo_: Object,

    /** @private */
    hasEndOfLife_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    eolMessageWithMonthAndYear_: {
      type: String,
      value: '',
    },

    /** @private */
    hasInternetConnection_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showCrostini: Boolean,

    /** @private */
    showCrostiniLicense_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showUpdateStatus_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showButtonContainer_: Boolean,

    /** @private */
    showRelaunch_: {
      type: Boolean,
      value: false,
      computed: 'computeShowRelaunch_(currentUpdateStatusEvent_)',
    },

    /** @private */
    showCheckUpdates_: {
      type: Boolean,
      computed: 'computeShowCheckUpdates_(' +
          'currentUpdateStatusEvent_, hasCheckedForUpdates_, hasEndOfLife_)',
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.DETAILED_BUILD_INFO) {
          map.set(
              settings.routes.DETAILED_BUILD_INFO.path,
              '#detailed-build-info-trigger');
        }
        return map;
      },
    },

    /** @private */
    showUpdateWarningDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showTPMFirmwareUpdateLineItem_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showTPMFirmwareUpdateDialog_: Boolean,

    /** @private {!AboutPageUpdateInfo|undefined} */
    updateInfo_: Object,

    /**
     * Whether the deep link to the check for OS update setting was unable to
     * be shown.
     * @private
     */
    isPendingOsUpdateDeepLink_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kCheckForOsUpdate,
        chromeos.settings.mojom.Setting.kSeeWhatsNew,
        chromeos.settings.mojom.Setting.kGetHelpWithChromeOs,
        chromeos.settings.mojom.Setting.kReportAnIssue,
        chromeos.settings.mojom.Setting.kTermsOfService,
      ]),
    },
  },

  observers: [
    'updateShowUpdateStatus_(' +
        'hasEndOfLife_, currentUpdateStatusEvent_,' +
        'hasCheckedForUpdates_)',
    'updateShowButtonContainer_(' +
        'showRelaunch_, showCheckUpdates_)',
    'handleCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
  ],

  /** @private {?settings.AboutPageBrowserProxy} */
  aboutBrowserProxy_: null,

  /** @private {?settings.LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @override */
  attached() {
    this.aboutBrowserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();
    this.aboutBrowserProxy_.pageReady();

    this.lifetimeBrowserProxy_ =
        settings.LifetimeBrowserProxyImpl.getInstance();

    this.addEventListener('target-channel-changed', e => {
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
    });

    this.aboutBrowserProxy_.checkInternetConnection().then(result => {
      this.hasInternetConnection_ = result;
    });

    if (settings.Router.getInstance().getQueryParameters().get(
            'checkForUpdate') == 'true') {
      this.onCheckUpdatesClick_();
    }
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    settings.MainPageBehavior.currentRouteChanged.call(
        this, newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== settings.routes.ABOUT_ABOUT) {
      return;
    }

    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Only the check for OS update is expected to fail deep link when
        // awaiting the check for update.
        assert(
            result.pendingSettingId ===
            chromeos.settings.mojom.Setting.kCheckForOsUpdate);
        this.isPendingOsUpdateDeepLink_ = true;
      }
    });
  },

  // Override settings.MainPageBehavior method.
  containsRoute(route) {
    return !route || settings.routes.ABOUT.contains(route);
  },

  /** @private */
  startListening_() {
    this.addWebUIListener(
        'update-status-changed', this.onUpdateStatusChanged_.bind(this));
    this.aboutBrowserProxy_.refreshUpdateStatus();
    this.addWebUIListener(
        'tpm-firmware-update-status-changed',
        this.onTPMFirmwareUpdateStatusChanged_.bind(this));
    this.aboutBrowserProxy_.refreshTPMFirmwareUpdateStatus();
  },

  /**
   * @param {!UpdateStatusChangedEvent} event
   * @private
   */
  onUpdateStatusChanged_(event) {
    if (event.status == UpdateStatus.CHECKING) {
      this.hasCheckedForUpdates_ = true;
    } else if (event.status == UpdateStatus.NEED_PERMISSION_TO_UPDATE) {
      this.showUpdateWarningDialog_ = true;
      this.updateInfo_ = {version: event.version, size: event.size};
    }
    this.currentUpdateStatusEvent_ = event;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreClick_(event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  },

  /** @private */
  onReleaseNotesTap_() {
    this.aboutBrowserProxy_.launchReleaseNotes();
  },

  /** @private */
  onHelpClick_() {
    this.aboutBrowserProxy_.openOsHelpPage();
  },

  /** @private */
  onRelaunchClick_() {
    settings.recordSettingChange();
    this.lifetimeBrowserProxy_.relaunch();
  },

  /** @private */
  updateShowUpdateStatus_() {
    // Do not show the "updated" status if we haven't checked yet or the update
    // warning dialog is shown to user.
    if (this.currentUpdateStatusEvent_.status == UpdateStatus.UPDATED &&
        (!this.hasCheckedForUpdates_ || this.showUpdateWarningDialog_)) {
      this.showUpdateStatus_ = false;
      return;
    }

    // Do not show "updated" status if the device is end of life.
    if (this.hasEndOfLife_) {
      this.showUpdateStatus_ = false;
      return;
    }

    this.showUpdateStatus_ =
        this.currentUpdateStatusEvent_.status != UpdateStatus.DISABLED;
  },

  /**
   * Hide the button container if all buttons are hidden, otherwise the
   * container displays an unwanted border (see separator class).
   * @private
   */
  updateShowButtonContainer_() {
    this.showButtonContainer_ = this.showRelaunch_ || this.showCheckUpdates_;

    // Check if we have yet to focus the check for update button.
    if (!this.isPendingOsUpdateDeepLink_) {
      return;
    }

    this.showDeepLink(chromeos.settings.mojom.Setting.kCheckForOsUpdate)
        .then(result => {
          if (result.deepLinkShown) {
            this.isPendingOsUpdateDeepLink_ = false;
          }
        });
  },

  /** @private */
  computeShowRelaunch_() {
    return this.checkStatus_(UpdateStatus.NEARLY_UPDATED);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowLearnMoreLink_() {
    return this.currentUpdateStatusEvent_.status == UpdateStatus.FAILED;
  },


  /**
   * @return {string}
   * @private
   */
  getUpdateStatusMessage_() {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.NEED_PERMISSION_TO_UPDATE:
        return this.i18nAdvanced('aboutUpgradeCheckStarted');
      case UpdateStatus.NEARLY_UPDATED:
        if (this.currentChannel_ != this.targetChannel_) {
          return this.i18nAdvanced('aboutUpgradeSuccessChannelSwitch');
        }
        if (this.currentUpdateStatusEvent_.rollback) {
          return this.i18nAdvanced('aboutRollbackSuccess');
        }
        return this.i18nAdvanced('aboutUpgradeRelaunch');
      case UpdateStatus.UPDATED:
        return this.i18nAdvanced('aboutUpgradeUpToDate');
      case UpdateStatus.UPDATING:
        assert(typeof this.currentUpdateStatusEvent_.progress == 'number');
        const progressPercent = this.currentUpdateStatusEvent_.progress + '%';

        if (this.currentChannel_ != this.targetChannel_) {
          return this.i18nAdvanced('aboutUpgradeUpdatingChannelSwitch', {
            substitutions: [
              this.i18nAdvanced(settings.browserChannelToI18nId(
                  this.targetChannel_, this.isLts_)),
              progressPercent
            ]
          });
        }
        if (this.currentUpdateStatusEvent_.rollback) {
          return this.i18nAdvanced('aboutRollbackInProgress', {
            substitutions: [progressPercent],
          });
        }
        if (this.currentUpdateStatusEvent_.progress > 0) {
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
        function formatMessage(msg) {
          return parseHtmlSubset('<b>' + msg + '</b>', ['br', 'pre'])
              .firstChild.innerHTML;
        }
        let result = '';
        const message = this.currentUpdateStatusEvent_.message;
        if (message) {
          result += formatMessage(message);
        }
        const connectMessage = this.currentUpdateStatusEvent_.connectionTypes;
        if (connectMessage) {
          result += '<div>' + formatMessage(connectMessage) + '</div>';
        }
        return result;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getUpdateStatusIcon_() {
    // If Chrome OS has reached end of life, display a special icon and
    // ignore UpdateStatus.
    if (this.hasEndOfLife_) {
      return 'os-settings:end-of-life';
    }

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case UpdateStatus.FAILED:
        return 'cr:error';
      case UpdateStatus.UPDATED:
      case UpdateStatus.NEARLY_UPDATED:
        // TODO(crbug.com/986596): Don't use browser icons here. Fork them.
        return 'settings:check-circle';
      default:
        return null;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getThrobberSrcIfUpdating_() {
    if (this.hasEndOfLife_) {
      return null;
    }

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

  /**
   * @param {!UpdateStatus} status
   * @return {boolean}
   * @private
   */
  checkStatus_(status) {
    return this.currentUpdateStatusEvent_.status == status;
  },

  /** @private */
  onManagementPageClick_() {
    window.open('chrome://management');
  },

  /**
   * @return {boolean}
   * @private
   */
  isPowerwash_() {
    return this.currentUpdateStatusEvent_.powerwash;
  },

  /** @private */
  onDetailedBuildInfoClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.DETAILED_BUILD_INFO);
  },

  /**
   * @return {string}
   * @private
   */
  getRelaunchButtonText_() {
    if (this.checkStatus_(UpdateStatus.NEARLY_UPDATED)) {
      if (this.isPowerwash_()) {
        return this.i18nAdvanced('aboutRelaunchAndPowerwash');
      } else {
        return this.i18nAdvanced('aboutRelaunch');
      }
    }
    return '';
  },

  /** @private */
  onCheckUpdatesClick_() {
    this.onUpdateStatusChanged_({status: UpdateStatus.CHECKING});
    this.aboutBrowserProxy_.requestUpdate();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowCheckUpdates_() {
    // Disable update button if the device is end of life.
    if (this.hasEndOfLife_) {
      return false;
    }

    // Enable the update button if we are in a stale 'updated' status or
    // update has failed. Disable it otherwise.
    const staleUpdatedStatus =
        !this.hasCheckedForUpdates_ && this.checkStatus_(UpdateStatus.UPDATED);

    return staleUpdatedStatus || this.checkStatus_(UpdateStatus.FAILED);
  },

  /**
   * @param {boolean} showCrostiniLicense True if Crostini is enabled and
   * Crostini UI is allowed.
   * @return {string}
   * @private
   */
  getAboutProductOsLicense_(showCrostiniLicense) {
    return showCrostiniLicense ?
        this.i18nAdvanced('aboutProductOsWithLinuxLicense') :
        this.i18nAdvanced('aboutProductOsLicense');
  },

  /**
   * @param {boolean} enabled True if Crostini is enabled.
   * @private
   */
  handleCrostiniEnabledChanged_(enabled) {
    this.showCrostiniLicense_ = enabled && this.showCrostini;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSafetyInfo_() {
    return loadTimeData.getBoolean('shouldShowSafetyInfo');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRegulatoryInfo_() {
    return this.regulatoryInfo_ !== null;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRegulatoryOrSafetyInfo_() {
    return this.shouldShowSafetyInfo_() || this.shouldShowRegulatoryInfo_();
  },

  /** @private */
  onUpdateWarningDialogClose_() {
    this.showUpdateWarningDialog_ = false;
    // Shows 'check for updates' button in case that the user cancels the
    // dialog and then intends to check for update again.
    this.hasCheckedForUpdates_ = false;
  },

  /**
   * @param {!TPMFirmwareUpdateStatusChangedEvent} event
   * @private
   */
  onTPMFirmwareUpdateStatusChanged_(event) {
    this.showTPMFirmwareUpdateLineItem_ = event.updateAvailable;
  },

  /** @private */
  onTPMFirmwareUpdateClick_() {
    this.showTPMFirmwareUpdateDialog_ = true;
  },

  /** @private */
  onPowerwashDialogClose_() {
    this.showTPMFirmwareUpdateDialog_ = false;
  },

  /** @private */
  onProductLogoClick_() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  },

  // <if expr="_google_chrome">
  /** @private */
  onReportIssueClick_() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  },
  // </if>

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIcons_() {
    if (this.hasEndOfLife_) {
      return true;
    }
    return this.showUpdateStatus_;
  },
});
