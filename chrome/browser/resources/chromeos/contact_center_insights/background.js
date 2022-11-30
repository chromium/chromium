// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bundle needs to be imported first so we can support closure style imports
// that follow
importScripts('ccaas_deps.js');

goog.require('proto.reporting.Record');
goog.require('proto.reporting.Destination');
goog.require('proto.reporting.Priority');
goog.require('proto.reporting.MetricData');
goog.require('proto.reporting.TelemetryData');
goog.require('proto.reporting.UserStatusTelemetry');
goog.require('proto.reporting.UserStatusTelemetry.DeviceActivityState');

const DEVICE_ACTIVITY_STATE_ALARM = 'DeviceActivityState';
const REPORT_DEVICE_ACTIVITY_STATE_PERIOD_MINUTES = 15;
const IDLE_THRESHOLD_SECONDS = 5 /** minutes **/ * 60;

function reportDeviceActivityState() {
  chrome.idle.queryState(IDLE_THRESHOLD_SECONDS, (state) => {
    const userStatusTelemetry = new proto.reporting.UserStatusTelemetry();
    const mappedState = getMappedDeviceActivityState(state);
    userStatusTelemetry.setDeviceActivityState(mappedState);

    const telemetryData = new proto.reporting.TelemetryData();
    telemetryData.setUserStatusTelemetry(userStatusTelemetry);

    reportTelemetryData(telemetryData);
  });
}

/**
 * Returns the internal representation for the current device activity state.
 * @param {string} activityState Device activity state.
 * @return {!proto.reporting.UserStatusTelemetry.DeviceActivityState} internal
 *     proto enum representation.
 */
function getMappedDeviceActivityState(activityState) {
  switch (activityState) {
    case 'active':
      return proto.reporting.UserStatusTelemetry.DeviceActivityState.ACTIVE;
    case 'idle':
      return proto.reporting.UserStatusTelemetry.DeviceActivityState.IDLE;
    case 'locked':
      return proto.reporting.UserStatusTelemetry.DeviceActivityState.LOCKED;
    default:
      return proto.reporting.UserStatusTelemetry.DeviceActivityState
          .DEVICE_ACTIVITY_STATE_UNKNOWN;
  }
}

/**
 * Reports collected telemetry data.
 * @param {!proto.reporting.TelemetryData} telemetryData Data to report.
 */
function reportTelemetryData(telemetryData) {
  const metricData = new proto.reporting.MetricData();
  metricData.setTelemetryData(telemetryData);

  // Also set metric timestamp (in ms) because the server also checks for these
  // in addition to the ones set with the record (in microseconds) today.
  const timestampMs = Date.now();
  metricData.setTimestampMs(timestampMs);

  const record = new proto.reporting.Record();
  record.setDestination(proto.reporting.Destination.TELEMETRY_METRIC);
  record.setData(metricData.serializeBinary());
  record.setTimestampUs(timestampMs * 1000);

  // Prepare enqueue record request
  const request = {
    recordData: record.serializeBinary(),
    priority: proto.reporting.Priority.FAST_BATCH,
    eventType: chrome.enterprise.reportingPrivate.EventType.USER,
  };

  // Report prepared request
  chrome.enterprise.reportingPrivate.enqueueRecord(request);
}

/**
 * Creates an alarm with specified polling interval if one is not registered
 * already.
 * @param {string} name Alarm name.
 * @param {number} periodInMinutes Polling interval in minutes
 */
function createAlarm(name, periodInMinutes) {
  chrome.alarms.get(name, (alarm) => {
    if (!alarm) {
      chrome.alarms.create(name, {periodInMinutes});
    }
  });
}

// Global listener for all alarms
chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === DEVICE_ACTIVITY_STATE_ALARM) {
    reportDeviceActivityState();
  }
});

// Register alarms for periodically reporting telemetry data.
chrome.runtime.onInstalled.addListener(() => {
  createAlarm(
      DEVICE_ACTIVITY_STATE_ALARM, REPORT_DEVICE_ACTIVITY_STATE_PERIOD_MINUTES);
});
