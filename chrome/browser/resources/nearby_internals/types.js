// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Severity enum based on LogMessage format. Needs to stay in sync with the
 * NearbyInternalsLogsHandler.
 * @enum {number}
 */
export const Severity = {
  VERBOSE: -1,
  INFO: 0,
  WARNING: 1,
  ERROR: 2,
};

/**
 * The type of log message object. The definition is based on
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_logs_handler.cc:
 * LogMessageToDictionary()
 * @typedef {{text: string,
 *            time: string,
 *            file: string,
 *            line: number,
 *            severity: Severity}}
 */
export let LogMessage;

/**
 * RPC enum based on the HTTP request/response object. Needs to stay in sync
 * with the NearbyInternalsHttpHandler C++ code, defined in
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 * @enum {number}
 */
export const Rpc = {
  CERTIFICATE: 0,
  CONTACT: 1,
  DEVICE: 2,
  DEVICE_STATE: 3,
};

/**
 * Direction enum based on the HTTP request/response object. Needs to stay in
 * sync with the NearbyInternalsHttpHandler C++ code, defined in
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 * @enum {number}
 */
export const Direction = {
  REQUEST: 0,
  RESPONSE: 1,
};

/**
 * The HTTP request/response object, sent by NearbyInternalsHttpHandler
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 * @typedef {{body: string,
 *            time: number,
 *            rpc: !Rpc,
 *            direction: !Direction}}
 */
export let HttpMessage;

/**
 * The ContactUpdate message object sent by NearbyInternalsContactsHandler
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_contact_handler.cc.
 * @typedef {{time: number,
 *            contactsListChanged: boolean,
 *            contactsAddedToAllowlist: boolean,
 *            contactsRemovedFromAllowlist: boolean,
 *            allowedIds: string,
 *            contactRecords: string}}
 */
export let ContactUpdate;

/**
 * The StatusCode callback object, sent by NearbyInternalsUiTriggerHandler
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_trigger_handler.cc.
 * @typedef {{statusCode: string,
 *            time: number,
 *            triggerEvent: string}}
 */
export let StatusCode;

/**
 * The TransferMetadata callback object, sent by NearbyInternalsUiTriggerHandler
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_trigger_handler.cc.
 * @typedef {{transferMetadataStatus: string,
 *            time: number,
 *            deviceName: string,
 *            shareTargetId: string}}
 */
export let TransferMetadataStatus;

/**
 * Timestamped message object that allows us to display information passed in
 * from the WebUIHandler in the list of the UI trigger tab.
 * @typedef {{message: string,
 *            time: number}}
 */
export let TimestampedMessage;


/**
 * A Nearby Presence Device object to be used for displaying nearby devices
 * during testing.
 * @typedef {{connectable: boolean,
 *            type: string,
 *            endpoint_id: string,
 *            actions: string}}
 */
export let PresenceDevice;

/**
 * Share Target object sent by NearbyInternalsUiTriggerHandler on discovery or
 * lost.
 * @typedef {{deviceName: string,
 *            shareTargetId: string,
 *            time: number}}
 */
export let ShareTarget;

/**
 * ShareTargetDiscoveryChange enum for display when ShareTarget is lost or
 * discovered.
 * @enum {number}
 */
export const ShareTargetDiscoveryChange = {
  DISCOVERED: 0,
  LOST: 1,
};

/**
 * Select object for displaying passed in ShareTargets in selection list.
 * @typedef {{name: string,
 *            selected: boolean,
 *            value: string}}
 */
export let ShareTargetSelectOption;

/**
 * Dictionary for Nearby Share State booleans
 * @typedef {{isScanning: boolean,
 *            isTransferring: boolean,
 *            isSending: boolean,
 *            isReceiving: boolean,
 *            isConnecting: boolean,
 *            isInHighVisibility: boolean,
 *            time: number}}
 */
export let NearbyShareStates;

/**
 * Object used by the logging tab to retrieve feature specific logs.
 * @typedef {{messageAddedEventName: string,
 *            bufferClearedEventName: string,
 *            logFilePrefix: string,
 *            getLogMessages: function(): Promise<!Array<!LogMessage>> }}
 */
export let LogProvider;
