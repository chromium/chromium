// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Power Control UI root element.
 */

/**
 * @type {Object}.
 * Currently loaded model.
 */
let activeModel = null;

/**
 * Sets current power control status.
 * @param {string} statusText text to set as a status.
 */
function setPowerControlStatus(statusText) {
  $('arc-power-control-status').textContent = statusText;
}

/**
 * Sets current tracing status.
 * @param {string} statusText text to set as a status.
 */
function setTracingStatus(statusText) {
  $('arc-tracing-status').textContent = statusText;
}

/**
 * Sets model to the view and refreshes everything.
 *
 * @param {object} model to display.
 */
function setModel(model) {
  activeModel = model;
  refreshModel();
}

/**
 * Sets throttling mode.
 *
 * @param {string} name of throttling mode.
 */
function setThrottlingMode(mode) {
  $('arc-power-control-throttling-disable').checked = false;
  $('arc-power-control-throttling-auto').checked = false;
  $('arc-power-control-throttling-force').checked = false;
  $('arc-power-control-throttling-' + mode).checked = true;
}

/**
 * Sets enable/disable power control
 *
 * @param {enable} true in case ARC power control enabled.
 */
function setPowerControlEnabled(enabled) {
  $('arc-power-control-buttons').hidden = !enabled;
}

function refreshModel() {
  // Clear previous content.
  $('arc-event-bands').textContent = '';

  if (!activeModel) {
    return;
  }
  const duration = activeModel.information.duration;

  // Microseconds per pixel. 100% zoom corresponds to 100 mcs per pixel.
  const resolution = zooms[zoomLevel];
  const parent = $('arc-event-bands');

  const topBandPadding = 4;
  const chartHeight = 96;
  const barHeight = 12;

  const controlTitle = new EventBandTitle(
      parent, undefined /* anchor */, 'Power control', 'arc-events-band-title');
  const controlBands =
      new EventBands(controlTitle, 'arc-events-band', resolution, 0, duration);
  controlBands.setWidth(controlBands.timestampToOffset(duration));
  controlBands.addChart(2 * barHeight, topBandPadding);

  const wakenessfullAttributes = {
    '-1': {color: '#ffc0cb', name: 'Unknown'},
    0: {color: '#5d5d5d', name: 'Asleep'},
    1: {color: '#fff0c7', name: 'Awake'},
    2: {color: '#c7cfff', name: 'Dreaming'},
    3: {color: '#3e49ed', name: 'Dozing'},
  };
  const throttlingAttributes = {
    0: {color: '#ffc0cb', name: 'Unknown'},
    1: {color: '#ccc', name: 'Throttling'},
    2: {color: '#7bed3e', name: 'Foreground'},
    3: {color: '#7bed3e', name: 'Foreground-important'},
    4: {color: '#7bed3e', name: 'Foreground-critical'},
  };

  controlBands.addBarSource(
      new Events(
          activeModel.system.memory, 14 /* kWakenessfullMode */,
          14 /* kWakenessfullMode */),
      wakenessfullAttributes, 0 /* y */, barHeight);
  controlBands.addBarSource(
      new Events(
          activeModel.system.memory, 15 /* kThrottlingMode */,
          15 /* kThrottlingMode */),
      throttlingAttributes, barHeight /* y */, barHeight);

  const cpusTitle = new EventBandTitle(
      parent, undefined /* anchor */, 'CPUs', 'arc-events-band-title');
  const cpusBands =
      new CpuEventBands(cpusTitle, 'arc-events-band', resolution, 0, duration);
  cpusBands.showDetailedInfo = false;
  cpusBands.setWidth(cpusBands.timestampToOffset(duration));
  cpusBands.setModel(activeModel);
  cpusBands.addChart(chartHeight, topBandPadding);
  cpusBands.addChartSources(
      [new Events(
          activeModel.system.memory, 8 /* kCpuTemperature */,
          8 /* kCpuTemperature */)],
      true /* smooth */);
  cpusBands.addChartSources(
      [new Events(
          activeModel.system.memory, 9 /* kCpuFrequency */,
          9 /* kCpuFrequency */)],
      true /* smooth */);
  cpusBands.addChartSources(
      [new Events(
          activeModel.system.memory, 10 /* kCpuPower */, 10 /* kCpuPower */)],
      true /* smooth */);

  const memoryTitle = new EventBandTitle(
      parent, undefined /* anchor */, 'Memory', 'arc-events-band-title');
  const memoryBands =
      new EventBands(memoryTitle, 'arc-events-band', resolution, 0, duration);
  memoryBands.setWidth(memoryBands.timestampToOffset(duration));
  memoryBands.addChart(chartHeight, topBandPadding);
  // Used memory chart.
  memoryBands.addChartSources(
      [new Events(
          activeModel.system.memory, 1 /* kMemUsed */, 1 /* kMemUsed */)],
      true /* smooth */);
  // Swap memory chart.
  memoryBands.addChartSources(
      [
        new Events(
            activeModel.system.memory, 2 /* kSwapRead */, 2 /* kSwapRead */),
        new Events(
            activeModel.system.memory, 3 /* kSwapWrite */, 3 /* kSwapWrite */),
      ],
      true /* smooth */);
  // Geom objects and size.
  memoryBands.addChartSources(
      [new Events(
          activeModel.system.memory, 5 /* kGemObjects */, 5 /* kGemObjects */)],
      true /* smooth */);
  memoryBands.addChartSources(
      [new Events(
          activeModel.system.memory, 6 /* kGemSize */, 6 /* kGemSize */)],
      true /* smooth */);
  memoryBands.addChartSources(
      [new Events(
          activeModel.system.memory, 12 /* kMemoryPower */,
          12 /* kMemoryPower */)],
      true /* smooth */);

  const chromeTitle = new EventBandTitle(
      parent, undefined /* anchor */, 'Chrome graphics',
      'arc-events-band-title');
  const chromeBands =
      new EventBands(chromeTitle, 'arc-events-band', resolution, 0, duration);
  chromeBands.setWidth(chromeBands.timestampToOffset(duration));
  chromeBands.addChart(chartHeight, topBandPadding);
  chromeBands.addChartSources(
      [new Events(
          activeModel.system.memory, 7 /* kGpuFrequency */,
          7 /* kGpuFrequency */)],
      false /* smooth */);
  chromeBands.addChartSources(
      [new Events(
          activeModel.system.memory, 11 /* kGpuPower */, 11 /* kGpuPower */)],
      true /* smooth */);
}

cr.define('cr.ArcPowerControl', function() {
  return {
    /**
     * Initializes internal structures.
     */
    initialize() {
      // Wakefulness mode
      $('arc-power-control-wakeup').onclick = function(event) {
        chrome.send('setWakefulnessMode', ['wakeup']);
      };
      $('arc-power-control-doze').onclick = function(event) {
        chrome.send('setWakefulnessMode', ['doze']);
      };
      $('arc-power-control-force-doze').onclick = function(event) {
        chrome.send('setWakefulnessMode', ['force-doze']);
      };
      $('arc-power-control-sleep').onclick = function(event) {
        chrome.send('setWakefulnessMode', ['sleep']);
      };
      // Throttling control
      $('arc-power-control-throttling-disable').onclick = function(event) {
        chrome.send('setThrottling', ['disable']);
      };
      $('arc-power-control-throttling-auto').onclick = function(event) {
        chrome.send('setThrottling', ['auto']);
      };
      $('arc-power-control-throttling-force').onclick = function(event) {
        chrome.send('setThrottling', ['force']);
      };
      // Tracing control
      $('arc-tracing-start').onclick = function(event) {
        chrome.send('startTracing');
      };
      $('arc-tracing-stop').onclick = function(event) {
        chrome.send('stopTracing');
      };

      initializeUi(10 /* zoomLevel */, function() {
        // Update function.
        refreshModel();
      });

      chrome.send('ready');
    },

    setPowerControlStatus: setPowerControlStatus,

    setTracingStatus: setTracingStatus,

    setThrottlingMode: setThrottlingMode,

    setPowerControlEnabled: setPowerControlEnabled,

    setModel: setModel,
  };
});

/**
 * Initializes UI.
 */
window.onload = function() {
  cr.ArcPowerControl.initialize();
};
