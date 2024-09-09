// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum of values to use for the feature select dropdown. If a new feature is
 * added, add it here.
 */
export enum FeatureValues {
  NEARBY_SHARE = 0,
  NEARBY_INFRA = 1,
  FAST_PAIR = 2,
}

/**
 * Enum of values to use for the action select dropdown. If a new action is
 * added, add it here.
 */
export enum ActionValues {
  START_SCAN = 0,
  STOP_SCAN = 1,
  SYNC_CREDENTIALS = 2,
  FIRST_TIME_FLOW = 3,
  RESET_NEARBY_SHARE = 4,
  ADD_PUSH_NOTIFICATION_CLIENT = 5,
  SHOW_RECEIVED_NOTIFICATION = 6,
  SEND_UPDATE_CREDENTIALS_MESSAGE = 7,
}

/**
 * Severity enum based on LogMessage format. Needs to stay in sync with the
 * NearbyInternalsLogsHandler.
 */
export enum Severity {
  VERBOSE = -1,
  INFO = 0,
  WARNING = 1,
  ERROR = 2,
}

/**
 * The interface of log message object. The definition is based on
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_logs_handler.cc:
 * LogMessageToDictionary()
 */
export interface LogMessage {
  text: string;
  feature: FeatureValues;
  time: string;
  file: string;
  line: number;
  severity: Severity;
}

/**
 * RPC enum based on the HTTP request/response object. Needs to stay in sync
 * with the NearbyInternalsHttpHandler C++ code, defined in
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 */
export enum Rpc {
  CERTIFICATE = 0,
  CONTACT = 1,
  DEVICE = 2,
  DEVICE_STATE = 3,
}

/**
 * Direction enum based on the HTTP request/response object. Needs to stay in
 * sync with the NearbyInternalsHttpHandler C++ code, defined in
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 */
export enum Direction {
  REQUEST = 0,
  RESPONSE = 1,
}

/**
 * The HTTP request/response object, sent by NearbyInternalsHttpHandler
 * chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.cc.
 */
export interface HttpMessage {
  body: string;
  time: number;
  rpc?: Rpc;
  direction?: Direction;
}

/**
 * The ContactUpdate message object sent by NearbyInternalsContactsHandler.
 */
export interface ContactUpdate {
  time: number;
  contactsListChanged: boolean;
  contactsAddedToAllowlist: boolean;
  contactsRemovedFromAllowlist: boolean;
  allowedIds: string;
  contactRecords: string;
}

/**
 * The StatusCode callback object, sent by NearbyInternalsUiTriggerHandler.
 */
export interface StatusCode {
  statusCode: string;
  time: number;
  triggerEvent: string;
}

/**
 * The TransferMetadata callback object, sent by
 * NearbyInternalsUiTriggerHandler.
 */
export interface TransferMetadataStatus {
  transferMetadataStatus: string;
  time: number;
  deviceName: string;
  shareTargetId: string;
}

/**
 * Timestamped message object that allows us to display information passed in
 * from the WebUIHandler in the list of the UI trigger tab.
 */
export interface TimestampedMessage {
  message: string;
  time: number;
}


/**
 * A Nearby Presence Device object to be used for displaying nearby devices
 * during testing.
 */
export interface PresenceDevice {
  connectable: boolean;
  type: string;
  endpoint_id: string;
  actions: string;
}

/**
 * Share Target object sent by NearbyInternalsUiTriggerHandler on discovery or
 * lost.
 */
export interface ShareTarget {
  deviceName: string;
  shareTargetId: string;
  time: number;
}

/**
 * ShareTargetDiscoveryChange enum for display when ShareTarget is lost or
 * discovered.
 */
export enum ShareTargetDiscoveryChange {
  DISCOVERED = 0,
  LOST = 1,
}

/**
 * Select object for displaying passed in ShareTargets in selection list.
 */
export interface ShareTargetSelectOption {
  name: string;
  selected: boolean;
  value: string;
}

/**
 * Dictionary for Nearby Share State booleans
 */
export interface NearbyShareStates {
  isScanning: boolean;
  isTransferring: boolean;
  isSending: boolean;
  isReceiving: boolean;
  isConnecting: boolean;
  isInHighVisibility: boolean;
  time: number;
}

/**
 * Object used by the logging tab to retrieve feature specific logs.
 */
export interface LogProvider {
  messageAddedEventName: string;
  bufferClearedEventName: string;
  logFilePrefix: string;
  getLogMessages: () => Promise<LogMessage[]>;
}

/**
 * Select object is used by the arrays which populate the actions drop down with
 * a list of actions specific to each feature.
 */
export interface SelectOption {
  name: string;
  value: string;
}
