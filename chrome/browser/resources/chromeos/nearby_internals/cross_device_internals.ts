// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import './np_list_object.js';
import './logging_tab.js';
import './log_object.js';
import './log_types.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cross_device_internals.html.js';
import {NearbyLogsBrowserProxy} from './cross_device_logs_browser_proxy.js';
import type {LogTypesElement} from './log_types.js';
import {NearbyPrefsBrowserProxy} from './nearby_prefs_browser_proxy.js';
import {NearbyPresenceBrowserProxy} from './nearby_presence_browser_proxy.js';
import {NearbyUiTriggerBrowserProxy} from './nearby_ui_trigger_browser_proxy.js';
import type {LogMessage, LogProvider, PresenceDevice, SelectOption} from './types.js';
import {ActionValues, FeatureValues, Severity} from './types.js';

/**
 * Converts log message to string format for saved download file.
 */
function logToSavedString(log: LogMessage): string {
  // Convert to string value for |line.severity|.
  let severity;
  switch (log.severity) {
    case Severity.INFO:
      severity = 'INFO';
      break;
    case Severity.WARNING:
      severity = 'WARNING';
      break;
    case Severity.ERROR:
      severity = 'ERROR';
      break;
    case Severity.VERBOSE:
      severity = 'VERBOSE';
      break;
  }

  // Reduce the file path to just the file name for logging simplification.
  const file = log.file.substring(log.file.lastIndexOf('/') + 1);

  return `[${log.time} ${severity} ${file} (${log.line})] ${log.text}\n`;
}

const CrossDeviceInternalsElementBase = WebUiListenerMixin(PolymerElement);

class CrossDeviceInternalsElement extends CrossDeviceInternalsElementBase {
  static get is() {
    return 'cross-device-internals';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {

      npDiscoveredDevicesList_: {
        type: Array,
        value: () => [],
      },

      featuresList_: {
        type: Array,
        value: [
          {name: 'Nearby Infra', value: FeatureValues.NEARBY_INFRA},
          {name: 'Nearby Share', value: FeatureValues.NEARBY_SHARE},
          {name: 'Fast Pair', value: FeatureValues.FAST_PAIR},
        ],
      },

      nearbyInfraActionList_: {
        type: Array,
        value: [
          {name: 'NP: Start Scan', value: ActionValues.START_SCAN},
          {name: 'NP: Stop Scan', value: ActionValues.STOP_SCAN},
          {name: 'NP: Sync Credentials', value: ActionValues.SYNC_CREDENTIALS},
          {name: 'NP: First time flow', value: ActionValues.FIRST_TIME_FLOW},
          {
            name: 'NP: Send Update Credentials Message',
            value: ActionValues.SEND_UPDATE_CREDENTIALS_MESSAGE,
          },
        ],
      },

      logLevelList_: {
        type: Array,
        value: [
          {name: 'VERBOSE', value: Severity.VERBOSE},
          {name: 'INFO', value: Severity.INFO},
          {name: 'WARNING', value: Severity.WARNING},
          {name: 'ERROR', value: Severity.ERROR},
        ],
      },

      nearbyShareActionList_: {
        type: Array,
        value: [
          {name: 'Reset Nearby Share', value: ActionValues.RESET_NEARBY_SHARE},
        ],
      },

      fastPairActionList_: {
        type: Array,
        value: () => [],
      },

      actionsSelectList_: {
        type: Array,
        value: () => [],
      },

      logList_: {
        type: Array,
        value: () => [],
      },

      filteredLogList_: {
        type: Array,
        value: () => [],
      },

      currentSeverity: {
        type: Severity,
        value: Severity.VERBOSE,
      },

      currentLogTypes: {
        type: FeatureValues,
        value: [
          FeatureValues.NEARBY_SHARE,
          FeatureValues.NEARBY_INFRA,
          FeatureValues.FAST_PAIR,
        ],
      },
    };
  }

  private npDiscoveredDevicesList_: PresenceDevice[];
  private featuresList_: SelectOption[];
  private nearbyInfraActionList_: SelectOption[];
  private nearbyShareActionList_: SelectOption[];
  private fastPairActionList_: SelectOption[];
  private actionsSelectList_: SelectOption[];
  private logList_: LogMessage[];
  private filteredLogList_: LogMessage[];
  private currentFilter_: string;
  private currentSeverity: Severity;
  private logLevelList_: SelectOption[];
  private logProvider_: LogProvider;
  private currentLogTypes: FeatureValues[];

  private nearbyPresenceBrowserProxy_: NearbyPresenceBrowserProxy =
      NearbyPresenceBrowserProxy.getInstance();
  private prefsBrowserProxy_: NearbyPrefsBrowserProxy =
      NearbyPrefsBrowserProxy.getInstance();
  private nearbyUITriggerBrowserProxy_: NearbyUiTriggerBrowserProxy =
      NearbyUiTriggerBrowserProxy.getInstance();

  /**
   * When the page is initialized, notify the C++ layer and load in the
   * contents of its log buffer. Initialize WebUI Listeners.
   */
  override connectedCallback() {
    super.connectedCallback();

    this.nearbyPresenceBrowserProxy_.initialize();
    this.nearbyUITriggerBrowserProxy_.initialize();
    this.addWebUiListener(
        'presence-device-found',
        (device: PresenceDevice) => this.onPresenceDeviceFound_(device));
    this.addWebUiListener(
        'presence-device-changed',
        (device: PresenceDevice) => this.onPresenceDeviceChanged_(device));
    this.addWebUiListener(
        'presence-device-lost',
        (device: PresenceDevice) => this.onPresenceDeviceLost_(device));
    this.set('actionsSelectList_', this.nearbyInfraActionList_);

    this.logProvider_ = {
      messageAddedEventName: 'log-message-added',
      bufferClearedEventName: 'log-buffer-cleared',
      logFilePrefix: 'cross_device_logs_',
      getLogMessages: () =>
          NearbyLogsBrowserProxy.getInstance().getLogMessages(),
    };
    this.addWebUiListener(
        this.logProvider_.messageAddedEventName,
        (log: LogMessage) => this.onLogMessageAdded_(log));
    this.addWebUiListener(
        this.logProvider_.bufferClearedEventName,
        () => this.onWebUiLogBufferCleared_());
    this.logProvider_.getLogMessages().then(
        (logs: LogMessage[]) => this.onGetLogMessages_(logs));
  }

  private updateActionsSelect_() {
    const actionGroup: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#actionGroup');

    if (actionGroup) {
      switch (Number(actionGroup.value)) {
        case FeatureValues.NEARBY_INFRA:
          this.set('actionsSelectList_', this.nearbyInfraActionList_);
          break;
        case FeatureValues.NEARBY_SHARE:
          this.set('actionsSelectList_', this.nearbyShareActionList_);
          break;
        case FeatureValues.FAST_PAIR:
          this.set('actionsSelectList_', this.fastPairActionList_);
          break;
      }
    }
  }

  private performAction_() {
    const actionSelect: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#actionSelect');
    if (actionSelect) {
      switch (Number(actionSelect.value)) {
        case ActionValues.START_SCAN:
          this.nearbyPresenceBrowserProxy_.sendStartScan();
          break;
        case ActionValues.STOP_SCAN:
          this.nearbyPresenceBrowserProxy_.sendStopScan();
          break;
        case ActionValues.SYNC_CREDENTIALS:
          this.nearbyPresenceBrowserProxy_.sendSyncCredentials();
          break;
        case ActionValues.FIRST_TIME_FLOW:
          this.nearbyPresenceBrowserProxy_.sendFirstTimeFlow();
          break;
        case ActionValues.RESET_NEARBY_SHARE:
          this.prefsBrowserProxy_.clearNearbyPrefs();
          break;
        case ActionValues.SEND_UPDATE_CREDENTIALS_MESSAGE:
          this.nearbyPresenceBrowserProxy_
              .sendUpdateCredentialsPushNotificationMessage();
          break;
        case ActionValues.SHOW_RECEIVED_NOTIFICATION:
          this.nearbyUITriggerBrowserProxy_
              .showNearbyShareReceivedNotification();
          break;
        default:
          break;
      }
    }
  }

  private onPresenceDeviceFound_(device: PresenceDevice): void {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    // If there is not a device with this endpoint_id currently in the devices
    // list, add it.
    if (!this.npDiscoveredDevicesList_.find(
            listDevice => listDevice.endpoint_id === endpointId)) {
      this.unshift('npDiscoveredDevicesList_', {
        'connectable': true,
        'type': type,
        'endpoint_id': endpointId,
        'actions': actions,
      });
    }
  }

  // TODO(b/277820435): Add and update device name for devices that have names
  // included.
  private onPresenceDeviceChanged_(device: PresenceDevice): void {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    const index = this.npDiscoveredDevicesList_.findIndex(
        listDevice => listDevice.endpoint_id === endpointId);

    // If a device was changed but we don't have a record of it being found,
    // add it to the array like performActiononPresenceDeviceFound__().
    if (index === -1) {
      this.unshift('npDiscoveredDevicesList_', {
        'connectable': true,
        'type': type,
        'endpoint_id': endpointId,
        'actions': actions,
      });
      return;
    }

    this.npDiscoveredDevicesList_[index] = {
      'connectable': true,
      'type': type,
      'endpoint_id': endpointId,
      'actions': actions,
    };
  }

  private onPresenceDeviceLost_(device: PresenceDevice): void {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    const index = this.npDiscoveredDevicesList_.findIndex(
        listDevice => listDevice.endpoint_id === endpointId);

    // The device was not found in the list.
    if (index === -1) {
      return;
    }

    this.npDiscoveredDevicesList_[index] = {
      'connectable': false,
      'type': type,
      'endpoint_id': endpointId,
      'actions': actions,
    };
  }


  /**
   * Clears javascript logs displayed, but c++ log buffer remains.
   */
  private onClearLogsButtonClicked_(): void {
    this.clearLogBuffer_();
  }

  /**
   * Saves and downloads all javascript logs.
   */
  private onSaveUnfilteredLogsButtonClicked_(): void {
    this.onSaveLogsButtonClicked_(false);
  }

  /**
   * Saves and downloads javascript logs that currently appear on the page.
   */
  private onSaveFilteredLogsButtonClicked_(): void {
    this.onSaveLogsButtonClicked_(true);
  }

  /**
   * Saves and downloads javascript logs.
   */
  private onSaveLogsButtonClicked_(filtered: boolean): void {
    let blob;
    if (filtered) {
      blob = new Blob(
          this.filteredLogList_.map(logToSavedString),
          {type: 'text/plain;charset=utf-8'});
    } else {
      blob = new Blob(
          this.logList_.map(logToSavedString),
          {type: 'text/plain;charset=utf-8'});
    }
    const url = URL.createObjectURL(blob);

    const anchorElement = document.createElement('a');
    anchorElement.href = url;
    anchorElement.download =
        this.logProvider_.logFilePrefix + new Date().toJSON() + '.txt';
    document.body.appendChild(anchorElement);
    anchorElement.click();

    window.setTimeout(function() {
      document.body.removeChild(anchorElement);
      window.URL.revokeObjectURL(url);
    }, 0);
  }

  /**
   * Adds a log message to the javascript log list displayed. Called from the
   * C++ WebUI handler when a log message is added to the log buffer.
   */
  private onLogMessageAdded_(log: LogMessage): void {
    this.push('logList_', log);
    if ((log.text.match(this.currentFilter_) ||
         log.file.match(this.currentFilter_)) &&
        log.severity >= this.currentSeverity &&
        this.currentLogTypes.includes(log.feature)) {
      this.push('filteredLogList_', log);
    }
  }

  private addLogFilter_(): void {
    const logLevelSelector: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#logLevelSelector');
    if (logLevelSelector) {
      switch (Number(logLevelSelector.value)) {
        case Severity.VERBOSE:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.VERBOSE));
          this.currentSeverity = Severity.VERBOSE;
          break;
        case Severity.INFO:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.INFO));
          this.currentSeverity = Severity.INFO;
          break;
        case Severity.WARNING:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.WARNING));
          this.currentSeverity = Severity.WARNING;
          break;
        case Severity.ERROR:
          this.set(
              'filteredLogList_',
              this.logList_.filter((log) => log.severity >= Severity.ERROR));
          this.currentSeverity = Severity.ERROR;
          break;
      }
    }
    const logType: LogTypesElement|null =
        this.shadowRoot!.querySelector('#logType');
    if (logType) {
      this.set(
          'currentLogTypes',
          logType.currentLogTypes,
      );
    }

    this.set(
        'filteredLogList_',
        this.filteredLogList_.filter(
            (log: LogMessage) => this.currentLogTypes.includes(log.feature)));

    const logSearch: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#logSearch');
    if (logSearch) {
      this.currentFilter_ = logSearch.value;
      this.set(
          'filteredLogList_',
          this.filteredLogList_.filter(
              (log: LogMessage) =>
                  (log.text.match(this.currentFilter_) ||
                   log.file.match(this.currentFilter_))));
    }
  }

  /**
   * Called in response to WebUI handler clearing log buffer.
   */
  private onWebUiLogBufferCleared_(): void {
    this.clearLogBuffer_();
  }

  /**
   * Parses an array of log messages and adds to the javascript list sent in
   * from the initial page load.
   */
  private onGetLogMessages_(logs: LogMessage[]): void {
    this.logList_ = logs.concat(this.logList_);
    this.filteredLogList_ = logs.slice();
  }

  /**
   * Clears the javascript log buffer.
   */
  private clearLogBuffer_(): void {
    this.logList_ = [];
    this.filteredLogList_ = [];
  }
}

customElements.define(
    CrossDeviceInternalsElement.is, CrossDeviceInternalsElement);
