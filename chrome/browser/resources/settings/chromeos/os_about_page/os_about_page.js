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
      value: function() {
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
    hasReleaseNotes_: {
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
    },

    /** @private */
    showRelaunchAndPowerwash_: {
      type: Boolean,
      value: false,
      computed: 'computeShowRelaunchAndPowerwash_(' +
          'currentUpdateStatusEvent_, targetChannel_, currentChannel_)',
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
      value: function() {
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
  },

  observers: [
    'updateShowUpdateStatus_(' +
        'hasEndOfLife_, currentUpdateStatusEvent_,' +
        'hasCheckedForUpdates_)',
    'updateShowRelaunch_(currentUpdateStatusEvent_, targetChannel_,' +
        'currentChannel_)',
    'updateShowButtonContainer_(' +
        'showRelaunch_, showRelaunchAndPowerwash_, showCheckUpdates_)',
    'handleCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
  ],

  /** @private {?settings.AboutPageBrowserProxy} */
  aboutBrowserProxy_: null,

  /** @private {?settings.LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @override */
  attached: function() {
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
      this.startListening_();
    });

    this.aboutBrowserProxy_.getRegulatoryInfo().then(info => {
      this.regulatoryInfo_ = info;
    });

    this.aboutBrowserProxy_.getEndOfLifeInfo().then(result => {
      this.hasEndOfLife_ = !!result.hasEndOfLife;
      this.eolMessageWithMonthAndYear_ = result.aboutPageEndOfLifeMessage || '';
    });

    this.aboutBrowserProxy_.getEnabledReleaseNotes().then(result => {
      this.hasReleaseNotes_ = result;
    });

    this.aboutBrowserProxy_.checkInternetConnection().then(result => {
      this.hasInternetConnection_ = result;
    });

    if (settings.getQueryParameters().get('checkForUpdate') == 'true') {
      this.onCheckUpdatesClick_();
    }
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    settings.MainPageBehavior.currentRouteChanged.call(
        this, newRoute, oldRoute);
  },

  // Override settings.MainPageBehavior method.
  containsRoute: function(route) {
    return !route || settings.routes.ABOUT.contains(route);
  },

  /** @private */
  startListening_: function() {
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
  onUpdateStatusChanged_: function(event) {
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
  onLearnMoreClick_: function(event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  },

  /** @private */
  onReleaseNotesTap_: function() {
    this.aboutBrowserProxy_.launchReleaseNotes();
  },

  /** @private */
  onHelpClick_: function() {
    this.aboutBrowserProxy_.openOsHelpPage();
  },

  /** @private */
  onRelaunchClick_: function() {
    this.lifetimeBrowserProxy_.relaunch();
  },

  /** @private */
  updateShowUpdateStatus_: function() {
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
  updateShowButtonContainer_: function() {
    this.showButtonContainer_ = this.showRelaunch_ ||
        this.showRelaunchAndPowerwash_ || this.showCheckUpdates_;
  },

  /** @private */
  updateShowRelaunch_: function() {
    this.showRelaunch_ =
        this.checkStatus_(UpdateStatus.NEARLY_UPDATED) && !this.isRollback_();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowLearnMoreLink_: function() {
    return this.currentUpdateStatusEvent_.status == UpdateStatus.FAILED;
  },

  /**
   * @return {string}
   * @private
   */
  getUpdateStatusMessage_: function() {
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
              this.i18nAdvanced(
                  settings.browserChannelToI18nId(this.targetChannel_)),
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
  getUpdateStatusIcon_: function() {
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
        return 'settings:check-circle';
      default:
        return null;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getThrobberSrcIfUpdating_: function() {
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
  checkStatus_: function(status) {
    return this.currentUpdateStatusEvent_.status == status;
  },

  /** @private */
  onManagementPageClick_: function() {
    window.open('chrome://management');
  },

  /**
   * @return {boolean}
   * @private
   */
  isRollback_: function() {
    assert(this.currentChannel_.length > 0);
    assert(this.targetChannel_.length > 0);
    if (this.currentUpdateStatusEvent_.rollback) {
      return true;
    }
    // Channel switch to a more stable channel is also a rollback
    return settings.isTargetChannelMoreStable(
        this.currentChannel_, this.targetChannel_);
  },

  /** @private */
  onDetailedBuildInfoClick_: function() {
    settings.navigateTo(settings.routes.DETAILED_BUILD_INFO);
  },

  /** @private */
  onRelaunchAndPowerwashClick_: function() {
    if (this.currentUpdateStatusEvent_.rollback) {
      // Wipe already initiated, simply relaunch.
      this.lifetimeBrowserProxy_.relaunch();
    } else {
      this.lifetimeBrowserProxy_.factoryReset(
          /* requestTpmFirmwareUpdate= */ false);
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowRelaunchAndPowerwash_: function() {
    return this.checkStatus_(UpdateStatus.NEARLY_UPDATED) && this.isRollback_();
  },

  /** @private */
  onCheckUpdatesClick_: function() {
    this.onUpdateStatusChanged_({status: UpdateStatus.CHECKING});
    this.aboutBrowserProxy_.requestUpdate();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowCheckUpdates_: function() {
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
  getAboutProductOsLicense_: function(showCrostiniLicense) {
    return showCrostiniLicense ?
        this.i18nAdvanced('aboutProductOsWithLinuxLicense') :
        this.i18nAdvanced('aboutProductOsLicense');
  },

  /**
   * @param {boolean} enabled True if Crostini is enabled.
   * @private
   */
  handleCrostiniEnabledChanged_: function(enabled) {
    this.showCrostiniLicense_ = enabled && this.showCrostini;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSafetyInfo_: function() {
    return loadTimeData.getBoolean('shouldShowSafetyInfo');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRegulatoryInfo_: function() {
    return this.regulatoryInfo_ !== null;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRegulatoryOrSafetyInfo_: function() {
    return this.shouldShowSafetyInfo_() || this.shouldShowRegulatoryInfo_();
  },

  /** @private */
  onUpdateWarningDialogClose_: function() {
    this.showUpdateWarningDialog_ = false;
    // Shows 'check for updates' button in case that the user cancels the
    // dialog and then intends to check for update again.
    this.hasCheckedForUpdates_ = false;
  },

  /**
   * @param {!TPMFirmwareUpdateStatusChangedEvent} event
   * @private
   */
  onTPMFirmwareUpdateStatusChanged_: function(event) {
    this.showTPMFirmwareUpdateLineItem_ = event.updateAvailable;
  },

  /** @private */
  onTPMFirmwareUpdateClick_: function() {
    this.showTPMFirmwareUpdateDialog_ = true;
  },

  /** @private */
  onPowerwashDialogClose_: function() {
    this.showTPMFirmwareUpdateDialog_ = false;
  },

  /** @private */
  onProductLogoClick_: function() {
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
  onReportIssueClick_: function() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  },
  // </if>

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIcons_: function() {
    if (this.hasEndOfLife_) {
      return true;
    }
    return this.showUpdateStatus_;
  },
});
