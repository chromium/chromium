// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Severity enum based on LogMessage format. Needs to stay in sync with the
 * MultideviceInternalsLogsHandler.
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
 * chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_logs_handler.cc:
 * LogMessageToDictionary()
 * @typedef {{text: string,
 *            time: string,
 *            file: string,
 *            line: number,
 *            severity: Severity}}
 */
export let LogMessage;

/**
 * Numerical values should not be changed because they must stay in sync with
 * chromeos/components/phonehub/feature_status.h.
 * @enum{number}
 */
export const FeatureStatus = {
  NOT_ELIGIBLE_FOR_FEATURE: 0,
  ELIGIBLE_PHONE_BUT_NOT_SETUP: 1,
  PHONE_SELECTED_AND_PENDING_SETUP: 2,
  DISABLED: 3,
  UNAVAILABLE_BLUETOOTH_OFF: 4,
  ENABLED_BUT_DISCONNECTED: 5,
  ENABLED_AND_CONNECTING: 6,
  ENABLED_AND_CONNECTED: 7,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * MobileStatus in chromeos/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const MobileStatus = {
  NO_SIM: 0,
  SIM_BUT_NO_RECEPTION: 1,
  SIM_WITH_RECEPTION: 2,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * SignalStrength in chromeos/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const SignalStrength = {
  ZERO_BARS: 0,
  ONE_BAR: 1,
  TWO_BARS: 2,
  THREE_BARS: 3,
  FOUR_BARS: 4,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * ChargingState inchromeos/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const ChargingState = {
  NOT_CHARGING: 0,
  CHARGING_AC: 1,
  CHARGING_USB: 2,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * BatterySaverState inchromeos/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const BatterySaverState = {
  OFF: 0,
  ON: 1,
};

/**
 * @typedef {{
 *   mobileStatus: !MobileStatus,
 *   signalStrength: !SignalStrength,
 *   mobileProvider: string,
 *   chargingState: !ChargingState,
 *   batterySaverState: !BatterySaverState,
 *   batteryPercentage: number,
 * }}
 */
export let PhoneStatusModel;
