// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import {KeyboardInfo, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';


/**
 * @typedef {{
 *   networkGuids: !Array<string>,
 *   activeGuid: string,
 * }}
 */
export let NetworkGuidInfo;

/**
 * Radio band related to channel frequency.
 * @enum {number}
 */
export const ChannelBand = {
  UNKNOWN: 0,
  /** 5Ghz radio band. */
  FIVE_GHZ: 1,
  /** 2.4Ghz radio band. */
  TWO_DOT_FOUR_GHZ: 2,
};

/**
 * Struct for holding data related to WiFi network channel.
 * @typedef {{
 *   channel: number,
 *   band: !ChannelBand,
 * }}
 */
export let ChannelProperties;

/**
 * @typedef {{
 *   routine: !RoutineType,
 *   blocking: boolean,
 * }}
 */
export let RoutineProperties;

/**
 * @typedef {{
 *   header: string,
 *   linkText: string,
 *   url: string,
 * }}
 */
export let TroubleshootingInfo;

/**
 * Type alias for ash::diagnostics::metrics::NavigationView to support message
 * handler logic and metric recording. Enum values need to be kept in sync with
 * "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h".
 * @enum {number}
 */
export const NavigationView = {
  kSystem: 0,
  kConnectivity: 1,
  kInput: 2,
  kMaxValue: 2,
};

/**
 * Type alias for the the response from InputDataProvider.GetConnectedDevices.
 * @typedef {{keyboards: !Array<!KeyboardInfo>,
 *            touchDevices: !Array<!TouchDeviceInfo>}}
 */
export let GetConnectedDevicesResponse;
