// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-detailed-build-info' contains detailed build
 * information for ChromeOS.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.m.js';
import '../../prefs/prefs.js';
import '../../settings_shared_css.js';
import '//resources/cr_components/localized_link/localized_link.js';
import './channel_switcher_dialog.js';
import './consumer_auto_update_toggle_dialog.js';
import './edit_hostname_dialog.js';

import {CrPolicyIndicatorType} from '//resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, browserChannelToI18nId, ChannelInfo, VersionInfo} from './about_page_browser_proxy.js';
import {DeviceNameBrowserProxy, DeviceNameBrowserProxyImpl, DeviceNameMetadata} from './device_name_browser_proxy.js';
import {DeviceNameState} from './device_name_util.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 */
const SettingsDetailedBuildInfoBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      WebUIListenerBehavior,
      I18nBehavior,
      PrefsBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsDetailedBuildInfoElement extends SettingsDetailedBuildInfoBase {
  static get is() {
    return 'settings-detailed-build-info';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private {!VersionInfo} */
      versionInfo_: Object,

      /** @private {!ChannelInfo} */
      channelInfo_: Object,

      /** @private {!DeviceNameMetadata} */
      deviceNameMetadata_: Object,

      /** @private */
      currentlyOnChannelText_: String,

      /** @private */
      showChannelSwitcherDialog_: Boolean,

      /** @private */
      showEditHostnameDialog_: Boolean,

      /** @private */
      canChangeChannel_: Boolean,

      /** @private */
      isManagedAutoUpdateEnabled_: Boolean,

      /** @private */
      showConsumerAutoUpdateToggleDialog_: Boolean,

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
          chromeos.settings.mojom.Setting.kChangeDeviceName,
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

      /**
       * Whether or not the consumer auto update toggling is allowed.
       * @private
       */
      isConsumerAutoUpdateTogglingAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isConsumerAutoUpdateTogglingAllowed');
        },
        readOnly: true,
      },

      /**
       * Whether or not to show the consumer auto update toggle.
       * @private
       */
      showConsumerAutoUpdateToggle_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showConsumerAutoUpdateToggle') &&
              !loadTimeData.getBoolean('isManaged');
        },
        readOnly: true,
      },
    };
  }

  constructor() {
    super();

    /** @private {!AboutPageBrowserProxy} */
    this.aboutPageBrowserProxy_ = AboutPageBrowserProxyImpl.getInstance();

    /** @private {!DeviceNameBrowserProxy} */
    this.deviceNameBrowserProxy_ = DeviceNameBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    this.aboutPageBrowserProxy_.pageReady();

    this.addEventListener('set-consumer-auto-update', e => {
      this.aboutPageBrowserProxy_.setConsumerAutoUpdate(e.detail.item);
    });

    if (this.isManaged_) {
      this.syncManagedAutoUpdateToggle_();
    } else {
      // This is to keep the Chrome pref in sync in case it becomes stale.
      // For example, if users toggle the consumer auto update, but the settings
      // page happened to crash/close before it got flushed out this would
      // assure a sync between the Chrome pref and the platform pref.
      this.syncConsumerAutoUpdateToggle_();
    }

    this.aboutPageBrowserProxy_.getVersionInfo().then(versionInfo => {
      this.versionInfo_ = versionInfo;
    });

    this.updateChannelInfo_();

    if (this.isHostnameSettingEnabled_) {
      this.addWebUIListener(
          'settings.updateDeviceNameMetadata',
          (data) => this.updateDeviceNameMetadata_(data));
      this.deviceNameBrowserProxy_.notifyReadyForDeviceName();
    }
  }

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.DETAILED_BUILD_INFO) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShouldHideEolInfo_() {
    return this.isManaged_ || !this.eolMessageWithMonthAndYear;
  }

  /** @private */
  updateChannelInfo_() {
    // canChangeChannel() call is expected to be low-latency, so fetch this
    // value by itself to ensure UI consistency (see https://crbug.com/848750).
    this.aboutPageBrowserProxy_.canChangeChannel().then(canChangeChannel => {
      this.canChangeChannel_ = canChangeChannel;
    });

    // getChannelInfo() may have considerable latency due to updates. Fetch this
    // metadata as part of a separate request.
    this.aboutPageBrowserProxy_.getChannelInfo().then(info => {
      this.channelInfo_ = info;
      // Display the target channel for the 'Currently on' message.
      this.currentlyOnChannelText_ = this.i18n(
          'aboutCurrentlyOnChannelInfo',
          this.i18n(browserChannelToI18nId(info.targetChannel, info.isLts)));
    });
  }

  /** @private */
  syncManagedAutoUpdateToggle_() {
    this.aboutPageBrowserProxy_.isManagedAutoUpdateEnabled().then(
        isManagedAutoUpdateEnabled => {
          this.isManagedAutoUpdateEnabled_ = isManagedAutoUpdateEnabled;
        });
  }

  /** @private */
  syncConsumerAutoUpdateToggle_() {
    this.aboutPageBrowserProxy_.isConsumerAutoUpdateEnabled().then(enabled => {
      this.aboutPageBrowserProxy_.setConsumerAutoUpdate(enabled);
    });
  }

  /**
   * @param {!DeviceNameMetadata} data
   * @private
   */
  updateDeviceNameMetadata_(data) {
    this.deviceNameMetadata_ = data;
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceNameText_() {
    if (!this.deviceNameMetadata_) {
      return '';
    }

    return this.deviceNameMetadata_.deviceName;
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceNameEditButtonA11yDescription_() {
    if (!this.deviceNameMetadata_) {
      return '';
    }

    return this.i18n(
        'aboutDeviceNameEditBtnA11yDescription', this.getDeviceNameText_());
  }

  /**
   * @return {boolean}
   * @private
   */
  canEditDeviceName_() {
    if (!this.deviceNameMetadata_) {
      return false;
    }

    return this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CAN_BE_MODIFIED;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPolicyIndicator_() {
    return this.getDeviceNameIndicatorType_() !== CrPolicyIndicatorType.NONE;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowConsumerAutoUpdateToggle_() {
    return !this.isManaged_;
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceNameIndicatorType_() {
    if (!this.deviceNameMetadata_) {
      return CrPolicyIndicatorType.NONE;
    }

    if (this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_OF_POLICIES) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }

    if (this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_NOT_DEVICE_OWNER) {
      return CrPolicyIndicatorType.OWNER;
    }

    return CrPolicyIndicatorType.NONE;
  }

  /**
   * @param {boolean} canChangeChannel
   * @return {string}
   * @private
   */
  getChangeChannelIndicatorSourceName_(canChangeChannel) {
    if (canChangeChannel) {
      // the indicator should be invisible.
      return '';
    }
    return loadTimeData.getBoolean('aboutEnterpriseManaged') ?
        '' :
        loadTimeData.getString('ownerEmail');
  }

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
  }

  /**
   * @param {!Event} e
   * @private
   */
  onChangeChannelTap_(e) {
    e.preventDefault();
    this.showChannelSwitcherDialog_ = true;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onEditHostnameTap_(e) {
    e.preventDefault();
    this.showEditHostnameDialog_ = true;
  }

  /**
   * @return {boolean}
   * @private
   */
  copyToClipBoardEnabled_() {
    return !!this.versionInfo_ && !!this.channelInfo_;
  }

  /** @private */
  onCopyBuildDetailsToClipBoardTap_() {
    const buildInfo = {
      'application_label': loadTimeData.getString('aboutBrowserVersion'),
      'platform': this.versionInfo_.osVersion,
      'aboutChannelLabel': this.channelInfo_.targetChannel,
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
  }

  /**
   * @param {!Event} e
   * @private
   */
  onConsumerAutoUpdateToggled_(e) {
    if (!this.isConsumerAutoUpdateTogglingAllowed_) {
      return;
    }
    this.showDialogOrFlushConsumerAutoUpdateToggle();
  }

  /** @private */
  onConsumerAutoUpdateToggledSettingsBox_() {
    if (!this.isConsumerAutoUpdateTogglingAllowed_) {
      return;
    }
    // Copy how cr-toggle negates the `checked` field.
    this.setPrefValue(
        'settings.consumer_auto_update_toggle',
        !this.getPref('settings.consumer_auto_update_toggle').value);
    this.showDialogOrFlushConsumerAutoUpdateToggle();
  }

  /** @private */
  showDialogOrFlushConsumerAutoUpdateToggle() {
    if (!this.getPref('settings.consumer_auto_update_toggle').value) {
      // Only show dialog when turning the toggle off.
      this.showConsumerAutoUpdateToggleDialog_ = true;
      return;
    }
    // Turning the toggle on requires no dialog.
    this.aboutPageBrowserProxy_.setConsumerAutoUpdate(true);
  }

  /** @private */
  onConsumerAutoUpdateToggleDialogClosed_() {
    this.showConsumerAutoUpdateToggleDialog_ = false;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onVisitBuildDetailsPageTap_(e) {
    e.preventDefault();
    window.open('chrome://version');
  }

  /** @private */
  onChannelSwitcherDialogClosed_() {
    this.showChannelSwitcherDialog_ = false;
    focusWithoutInk(assert(this.shadowRoot.querySelector('cr-button')));
    this.updateChannelInfo_();
  }

  /** @private */
  onEditHostnameDialogClosed_() {
    this.showEditHostnameDialog_ = false;
    focusWithoutInk(assert(this.shadowRoot.querySelector('cr-button')));
  }
}

customElements.define(
    SettingsDetailedBuildInfoElement.is, SettingsDetailedBuildInfoElement);
