// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import './np_list_object.js';
import './logging_tab.js';
import './log_object.js';
import './log_types.js';
import '//resources/cr_elements/md_select.css.js';
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {chimeBrowserProxy} from './chime_browser_proxy.js';
import {getTemplate} from './cross_device_internals.html.js';
import {NearbyLogsBrowserProxy} from './cross_device_logs_browser_proxy.js';
import {NearbyPrefsBrowserProxy} from './nearby_prefs_browser_proxy.js';
import {NearbyPresenceBrowserProxy} from './nearby_presence_browser_proxy.js';
import {ActionValues, FeatureValues, LogMessage, LogProvider, PresenceDevice, SelectOption, Severity} from './types.js';


/**
 * Converts log message to string format for saved download file.
 * @param {!LogMessage} log
 * @return {string}
 */
function logToSavedString_(log) {
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

Polymer({
  is: 'cross-device-internals',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],

  /** @private {?NearbyPresenceBrowserProxy} */
  nearbyPresenceBrowserProxy_: null,

  /** @private {?chimeBrowserProxy} */
  chimeBrowserProxy_: null,

  /** @private {?NearbyPrefsBrowserProxy}*/
  prefsBrowserProxy_: null,

  properties: {
    /** @private {!Array<!PresenceDevice>} */
    npDiscoveredDevicesList_: {
      type: Array,
      value: [],
    },

    /** @private {!Array<!SelectOption>} */
    featuresList: {
      type: Array,
      value: [
        {name: 'Nearby Presence', value: FeatureValues.NEARBY_PRESENCE},
        {name: 'Nearby Share', value: FeatureValues.NEARBY_SHARE},
        {name: 'Nearby Connections', value: FeatureValues.NEARBY_CONNECTIONS},
        {name: 'Fast Pair', value: FeatureValues.FAST_PAIR},
        {name: 'Chime', value: FeatureValues.CHIME},
      ],
    },

    /** @private {!Array<!SelectOption>} */
    nearbyPresenceActionList: {
      type: Array,
      value: [
        {name: 'Start Scan', value: ActionValues.START_SCAN},
        {name: 'Stop Scan', value: ActionValues.STOP_SCAN},
        {name: 'Sync Credentials', value: ActionValues.SYNC_CREDENTIALS},
        {name: 'First time flow', value: ActionValues.FIRST_TIME_FLOW},
      ],
    },

    /** @private {!Array<!SelectOption>} */
    logLevelList: {
      type: Array,
      value: [
        {name: 'VERBOSE', value: Severity.VERBOSE},
        {name: 'INFO', value: Severity.INFO},
        {name: 'WARNING', value: Severity.WARNING},
        {name: 'ERROR', value: Severity.ERROR},
      ],
    },

    /** @private {!Array<!SelectOption>} */
    nearbyShareActionList: {
      type: Array,
      value: [
        {name: 'Reset Nearby Share', value: ActionValues.RESET_NEARBY_SHARE},
      ],
    },

    /** @private {!Array<!SelectOption>} */
    nearbyConnectionsActionList: {
      type: Array,
      value: [],
    },

    /** @private {!Array<!SelectOption>} */
    fastPairActionList: {
      type: Array,
      value: [],
    },

    /** @private {!Array<!SelectOption>} */
    chimeActionList: {
      type: Array,
      value: [
        {name: 'Add Chime Client', value: ActionValues.ADD_CHIME_CLIENT},
      ],
    },

    /** @private {!Array<!SelectOption>} */
    actionsSelectList: {
      type: Array,
      value: [],
    },

    /**
     * @private {!Array<!LogMessage>}
     */
    logList_: {
      type: Array,
      value: [],
    },

    /**
     * @private {!Array<!LogMessage>}
     */
    filteredLogList_: {
      type: Array,
      value: [],
    },

    /** @private {!string} */
    feature: {
      type: String,
    },

    /** @private {!string} */
    currentFilter: {
      type: String,
    },

    /** @private {!Severity} */
    currentSeverity: {
      type: Severity,
      value: Severity.VERBOSE,
    },

    /** @private {!Array<FeatureValues>} */
    currentLogTypes: {
      type: FeatureValues,
      value: [
        FeatureValues.NEARBY_SHARE,
        FeatureValues.NEARBY_CONNECTIONS,
        FeatureValues.NEARBY_PRESENCE,
        FeatureValues.FAST_PAIR,
      ],
    },
  },


  /** @private {?LogProvider}*/
  logProvider_: null,

  created() {
    this.nearbyPresenceBrowserProxy_ = NearbyPresenceBrowserProxy.getInstance();
    this.chimeBrowserProxy_ = chimeBrowserProxy.getInstance();
    this.prefsBrowserProxy_ = NearbyPrefsBrowserProxy.getInstance();
  },

  /**
   * When the page is initialized, notify the C++ layer and load in the
   * contents of its log buffer. Initialize WebUI Listeners.
   * @override
   */
  attached() {
    this.nearbyPresenceBrowserProxy_.initialize();
    this.chimeBrowserProxy_.initialize();
    this.addWebUIListener(
        'presence-device-found', device => this.onPresenceDeviceFound_(device));
    this.addWebUIListener(
        'presence-device-changed',
        device => this.onPresenceDeviceChanged_(device));
    this.addWebUIListener(
        'presence-device-lost', device => this.onPresenceDeviceLost_(device));
    this.set('actionsSelectList', this.nearbyPresenceActionList);

    this.logProvider_ = {
      messageAddedEventName: 'log-message-added',
      bufferClearedEventName: 'log-buffer-cleared',
      logFilePrefix: 'cross_device_logs_',
      getLogMessages: () =>
          NearbyLogsBrowserProxy.getInstance().getLogMessages(),
    };
    this.addWebUIListener(
        this.logProvider_.messageAddedEventName,
        log => this.onLogMessageAdded_(log));
    this.addWebUIListener(
        this.logProvider_.bufferClearedEventName,
        () => this.onWebUILogBufferCleared_());
    this.logProvider_.getLogMessages().then(
        logs => this.onGetLogMessages_(logs));
  },

  updateActionsSelect() {
    switch (Number(this.$.actionGroup.value)) {
      case FeatureValues.NEARBY_PRESENCE:
        this.set('actionsSelectList', this.nearbyPresenceActionList);
        break;
      case FeatureValues.NEARBY_CONNECTIONS:
        this.set('actionsSelectList', this.nearbyConnectionsActionList);
        break;
      case FeatureValues.NEARBY_SHARE:
        this.set('actionsSelectList', this.nearbyShareActionList);
        break;
      case FeatureValues.FAST_PAIR:
        this.set('actionsSelectList', this.fastPairActionList);
        break;
      case FeatureValues.CHIME:
        this.set('actionsSelectList', this.chimeActionList);
        break;
    }
  },

  perform_action() {
    switch (Number(this.$.actionSelect.value)) {
      case ActionValues.START_SCAN:
        this.nearbyPresenceBrowserProxy_.SendStartScan();
        break;
      case ActionValues.STOP_SCAN:
        this.nearbyPresenceBrowserProxy_.SendStopScan();
        break;
      case ActionValues.SYNC_CREDENTIALS:
        this.nearbyPresenceBrowserProxy_.SendSyncCredentials();
        break;
      case ActionValues.FIRST_TIME_FLOW:
        this.nearbyPresenceBrowserProxy_.SendFirstTimeFlow();
        break;
      case ActionValues.RESET_NEARBY_SHARE:
        this.prefsBrowserProxy_.clearNearbyPrefs();
        break;
      case ActionValues.ADD_CHIME_CLIENT:
        this.chimeBrowserProxy_.SendAddChimeClient();
      default:
        break;
    }
  },

  onPresenceDeviceFound_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    // If there is not a device with this endpoint_id currently in the devices
    // list, add it.
    if (!this.npDiscoveredDevicesList_.find(
            list_device => list_device.endpoint_id === endpointId)) {
      this.unshift('npDiscoveredDevicesList_', {
        'connectable': true,
        'type': type,
        'endpoint_id': endpointId,
        'actions': actions,
      });
    }
  },

  // TODO(b/277820435): Add and update device name for devices that have names
  // included.
  onPresenceDeviceChanged_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    const index = this.npDiscoveredDevicesList_.findIndex(
        list_device => list_device.endpoint_id === endpointId);

    // If a device was changed but we don't have a record of it being found,
    // add it to the array like onPresenceDeviceFound_().
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
  },

  onPresenceDeviceLost_(device) {
    const type = device['type'];
    const endpointId = device['endpoint_id'];
    const actions = device['actions'];

    const index = this.npDiscoveredDevicesList_.findIndex(
        list_device => list_device.endpoint_id === endpointId);

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
  },


  /**
   * Clears javascript logs displayed, but c++ log buffer remains.
   * @private
   */
  onClearLogsButtonClicked_() {
    this.clearLogBuffer_();
  },

  /**
   * Saves and downloads all javascript logs.
   * @private
   */
  onSaveUnfilteredLogsButtonClicked_() {
    this.onSaveLogsButtonClicked_(false);
  },

  /**
   * Saves and downloads javascript logs that currently appear on the page.
   * @private
   */
  onSaveFilteredLogsButtonClicked_() {
    this.onSaveLogsButtonClicked_(true);
  },

  /**
   * Saves and downloads javascript logs.
   * @param {!boolean} filtered
   * @private
   */
  onSaveLogsButtonClicked_(filtered) {
    let blob;
    if (filtered) {
      blob = new Blob(
          this.filteredLogList_.map(logToSavedString_),
          {type: 'text/plain;charset=utf-8'});
    } else {
      blob = new Blob(
          this.logList_.map(logToSavedString_),
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
  },

  /**
   * Adds a log message to the javascript log list displayed. Called from the
   * C++ WebUI handler when a log message is added to the log buffer.
   * @param {!LogMessage} log
   * @private
   */
  onLogMessageAdded_(log) {
    this.push('logList_', log);
    if ((log.text.match(this.currentFilter) ||
         log.file.match(this.currentFilter)) &&
        log.severity >= this.currentSeverity &&
        this.currentLogTypes.includes(log.feature)) {
      this.push('filteredLogList_', log);
    }
  },

  addLogFilter() {
    switch (Number(this.$.logLevelSelector.value)) {
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

    this.set(
        'currentLogTypes',
        this.$.logType.currentLogTypes,
    );

    this.set(
        'filteredLogList_',
        this.filteredLogList_.filter(
            (log) => this.currentLogTypes.includes(log.feature)));

    this.currentFilter = this.$.logSearch.value;
    this.set(
        'filteredLogList_',
        this.filteredLogList_.filter(
            (log) =>
                (log.text.match(this.currentFilter) ||
                 log.file.match(this.currentFilter))));
  },

  /**
   * Called in response to WebUI handler clearing log buffer.
   * @private
   */
  onWebUILogBufferCleared_() {
    this.clearLogBuffer_();
  },

  /**
   * Parses an array of log messages and adds to the javascript list sent in
   * from the initial page load.
   * @param {!Array<!LogMessage>} logs
   * @private
   */
  onGetLogMessages_(logs) {
    this.logList_ = logs.concat(this.logList_);
    this.filteredLogList_ = logs.slice();
  },

  /**
   * Clears the javascript log buffer.
   * @private
   */
  clearLogBuffer_() {
    this.logList_ = [];
    this.filteredLogList_ = [];
  },
});
