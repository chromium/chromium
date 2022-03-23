// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bundle needs to be imported first so we can support closure style imports
// that follow
importScripts('ccaas_deps.js');

goog.require('proto.reporting.BandwidthData');
goog.require('proto.reporting.Record');
goog.require('proto.reporting.Destination');
goog.require('proto.reporting.Priority');
goog.require('proto.reporting.NetworksTelemetry');

const NETWORK_BANDWIDTH_ALARM = 'NetworkBandwidth';
const REPORT_NETWORK_BANDWIDTH_PERIOD_MINUTES = 12 /** hours **/ * 60;

function reportBandwidthData() {
  // Extract bandwidth data
  const networkInfo = navigator.connection;
  if (!networkInfo) {
    // No data
    console.error('Network info unavailable');
    return;
  }

  // Prepare telemetry proto message with network bandwidth information
  const bandwidth = new proto.reporting.BandwidthData();
  const downloadSpeedKbps = networkInfo.downlink /** mbps **/ * 1000;
  bandwidth.setDownloadSpeedKbps(downloadSpeedKbps);

  const telemetryData = new proto.reporting.NetworksTelemetry();
  telemetryData.setBandwidthData(bandwidth);

  const record = new proto.reporting.Record();
  record.setDestination(proto.reporting.Destination.TELEMETRY_METRIC);
  record.setData(telemetryData.serializeBinary());

  // Prepare enqueue record request
  const request = {
    recordData: record.serializeBinary(),
    priority: proto.reporting.Priority.FAST_BATCH,
    eventType: chrome.enterprise.reportingPrivate.EventType.USER
  };

  // Report prepared request
  chrome.enterprise.reportingPrivate.enqueueRecord(request);
}

// Global listener for all alarms
chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === NETWORK_BANDWIDTH_ALARM) {
    reportBandwidthData();
  }
});

// Register alarm for periodically reporting network bandwidth
chrome.runtime.onInstalled.addListener(() => {
  chrome.alarms.get(NETWORK_BANDWIDTH_ALARM, (alarm) => {
    if (!alarm) {
      chrome.alarms.create(
          NETWORK_BANDWIDTH_ALARM,
          {periodInMinutes: REPORT_NETWORK_BANDWIDTH_PERIOD_MINUTES});
    }
  });
});
