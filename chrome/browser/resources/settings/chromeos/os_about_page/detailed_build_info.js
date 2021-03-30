// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-detailed-build-info' contains detailed build
 * information for ChromeOS.
 */

Polymer({
  is: 'settings-detailed-build-info',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** @private {!VersionInfo} */
    versionInfo_: Object,

    /** @private {!ChannelInfo} */
    channelInfo_: Object,

    /** @private */
    currentlyOnChannelText_: String,

    /** @private */
    deviceNameText_: String,

    /** @private */
    showChannelSwitcherDialog_: Boolean,

    /** @private */
    showEditHostnameDialog_: Boolean,

    /** @private */
    canChangeChannel_: Boolean,

    eolMessageWithMonthAndYear: {
      type: String,
      value: '',
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kChangeChromeChannel,
        chromeos.settings.mojom.Setting.kCopyDetailedBuildInfo,
      ]),
    },

    /** @private */
    shouldHideEolInfo_: {
      type: Boolean,
      computed: 'computeShouldHideEolInfo_(eolMessageWithMonthAndYear)',
    },

    /** @private */
    isHostnameSettingEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isHostnameSettingEnabled');
      },
      readOnly: true,
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
      readOnly: true,
    },
  },

  /** @override */
  ready() {
    const browserProxy = settings.AboutPageBrowserProxyImpl.getInstance();
    browserProxy.pageReady();

    browserProxy.getVersionInfo().then(versionInfo => {
      this.versionInfo_ = versionInfo;
    });

    this.updateChannelInfo_();

    if (this.isHostnameSettingEnabled_) {
      this.updateDeviceName_();
    }
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.DETAILED_BUILD_INFO) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldHideEolInfo_() {
    return this.isManaged_ || !this.eolMessageWithMonthAndYear;
  },

  /** @private */
  updateChannelInfo_() {
    const browserProxy = settings.AboutPageBrowserProxyImpl.getInstance();

    // canChangeChannel() call is expected to be low-latency, so fetch this
    // value by itself to ensure UI consistency (see https://crbug.com/848750).
    browserProxy.canChangeChannel().then(canChangeChannel => {
      this.canChangeChannel_ = canChangeChannel;
    });

    // getChannelInfo() may have considerable latency due to updates. Fetch this
    // metadata as part of a separate request.
    browserProxy.getChannelInfo().then(info => {
      this.channelInfo_ = info;
      // Display the target channel for the 'Currently on' message.
      this.currentlyOnChannelText_ = this.i18n(
          'aboutCurrentlyOnChannel',
          this.i18n(
              settings.browserChannelToI18nId(info.targetChannel, info.isLts)));
    });
  },

  /** @private */
  updateDeviceName_() {
    const browserProxy = DeviceNameBrowserProxyImpl.getInstance();
    browserProxy.getDeviceNameMetadata().then(data => {
      this.deviceNameText_ = data.deviceName;
    });
  },

  /**
   * @param {boolean} canChangeChannel
   * @return {string}
   * @private
   */
  getChangeChannelIndicatorSourceName_(canChangeChannel) {
    return loadTimeData.getBoolean('aboutEnterpriseManaged') ?
        '' :
        loadTimeData.getString('ownerEmail');
  },

  /**
   * @param {boolean} canChangeChannel
   * @return {CrPolicyIndicatorType}
   * @private
   */
  getChangeChannelIndicatorType_(canChangeChannel) {
    if (canChangeChannel) {
      return CrPolicyIndicatorType.NONE;
    }
    return loadTimeData.getBoolean('aboutEnterpriseManaged') ?
        CrPolicyIndicatorType.DEVICE_POLICY :
        CrPolicyIndicatorType.OWNER;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onChangeChannelTap_(e) {
    e.preventDefault();
    this.showChannelSwitcherDialog_ = true;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEditHostnameTap_(e) {
    e.preventDefault();
    this.showEditHostnameDialog_ = true;
  },

  /**
   * @return {boolean}
   * @private
   */
  copyToClipBoardEnabled_: function() {
    return !!this.versionInfo_ && !!this.channelInfo_;
  },

  /** @private */
  onCopyBuildDetailsToClipBoardTap_: function() {
    const buildInfo = {
      'application_label': loadTimeData.getString('aboutBrowserVersion'),
      'platform': this.versionInfo_.osVersion,
      'aboutChannelLabel': this.channelInfo_.targetChannel +
          (this.channelInfo_.isLts ? ' (trusted tester)' : ''),
      'firmware_version': this.versionInfo_.osFirmware,
      'aboutIsArcStatusTitle': loadTimeData.getBoolean('aboutIsArcEnabled'),
      'arc_label': this.versionInfo_.arcVersion,
      'isEnterpriseManagedTitle':
          loadTimeData.getBoolean('aboutEnterpriseManaged'),
      'aboutIsDeveloperModeTitle':
          loadTimeData.getBoolean('aboutIsDeveloperMode'),
    };

    const entries = [];
    for (const key in buildInfo) {
      entries.push(this.i18n(key) + ': ' + buildInfo[key]);
    }

    navigator.clipboard.writeText(entries.join('\n'));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onVisitBuildDetailsPageTap_(e) {
    e.preventDefault();
    window.open('chrome://version');
  },

  /** @private */
  onChannelSwitcherDialogClosed_() {
    this.showChannelSwitcherDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('cr-button')));
    this.updateChannelInfo_();
  },

  /** @private */
  onEditHostnameDialogClosed_() {
    this.showEditHostnameDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('cr-button')));
    // TODO(jhawkins): Verify hostname property updated at this point.
  },
});
