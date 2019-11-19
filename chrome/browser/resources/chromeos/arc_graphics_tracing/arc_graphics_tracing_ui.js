// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Graphics Tracing UI.
 */

/**
 * @type {Object}.
 * Currently loaded model.
 */
var activeModel = null;

/**
 * Initialises graphic tracing UI. It calls initialization of base tracing UI
 * and additionally sets handler for the model saving.
 */
function initializeGraphicsUi() {
  initializeUi(5 /* zoomLevel */, function() {
    // Update function.
    if (activeModel) {
      setGraphicBuffersModel(activeModel);
    }
  });

  $('arc-graphics-tracing-save').onclick = function(event) {
    var linkElement = document.createElement('a');
    var file = new Blob([JSON.stringify(activeModel)], {type: 'text/plain'});
    linkElement.href = URL.createObjectURL(file);
    linkElement.download = 'tracing_model.json';
    linkElement.click();
  };
}

function setModelHeader(model) {
  $('arc-graphics-tracing-icon').src = '';
  $('arc-graphics-tracing-title').textContent = '';

  if (!model.information) {
    return;
  }

  if (model.information.icon) {
    $('arc-graphics-tracing-icon').src =
        'data:image/png;base64,' + model.information.icon;
  }
  if (model.information.title) {
    var title = model.information.title;
    if (model.information.timestamp) {
      title += ' ';
      title += new Date(model.information.timestamp).toLocaleString();
    }
    title += ' ';
    title += (model.information.duration * 0.000001).toFixed(2);
    title += 's';
    if (model.information.platform) {
      title += ' on ';
      title += model.information.platform;
    }
    title += '.';
    $('arc-graphics-tracing-title').textContent = title;
  }
}

/**
 * Creates visual representation of graphic buffers event model.
 *
 * @param {Object} model object produced by |ArcTracingGraphicsModel|.
 */
function setGraphicBuffersModel(model) {
  // Clear previous content.
  $('arc-event-bands').textContent = '';
  activeModel = model;

  setModelHeader(model);

  var duration = model.information.duration;

  // Microseconds per pixel. 100% zoom corresponds to 100 mcs per pixel.
  var resolution = zooms[zoomLevel];
  var parent = $('arc-event-bands');

  var topBandHeight = 16;
  var topBandPadding = 4;
  var innerBandHeight = 12;
  var innerBandPadding = 2;
  var innerLastBandPadding = 12;
  var chartHeight = 48;

  var vsyncEvents = new Events(
      model.android.global_events, 406 /* kVsyncTimestamp */,
      406 /* kVsyncTimestamp */);

  var cpusTitle = new EventBandTitle(parent, 'CPUs', 'arc-events-band-title');
  var cpusBands =
      new CpuEventBands(cpusTitle, 'arc-events-band', resolution, 0, duration);
  cpusBands.setWidth(cpusBands.timestampToOffset(duration));
  cpusBands.setModel(model);
  cpusBands.addChartToExistingArea(0 /* top */, cpusBands.height);
  cpusBands.addChartSources(
      [new Events(
          model.system.memory, 8 /* kCpuTemperature */,
          8 /* kCpuTemperature */)],
      true /* smooth */);
  cpusBands.addChartSources(
      [new Events(
          model.system.memory, 9 /* kCpuFrequency */, 9 /* kCpuFrequency */)],
      true /* smooth */);
  cpusBands.addChartSources(
      [new Events(model.system.memory, 10 /* kCpuPower */, 10 /* kCpuPower */)],
      true /* smooth */);
  cpusBands.setVSync(vsyncEvents);

  var memoryTitle =
      new EventBandTitle(parent, 'Memory', 'arc-events-band-title');
  var memoryBands =
      new EventBands(memoryTitle, 'arc-events-band', resolution, 0, duration);
  memoryBands.setWidth(memoryBands.timestampToOffset(duration));
  memoryBands.addChart(chartHeight, topBandPadding);
  // Used memory chart.
  memoryBands.addChartSources(
      [new Events(model.system.memory, 1 /* kMemUsed */, 1 /* kMemUsed */)],
      true /* smooth */);
  // Swap memory chart.
  memoryBands.addChartSources(
      [
        new Events(model.system.memory, 2 /* kSwapRead */, 2 /* kSwapRead */),
        new Events(
            model.system.memory, 3 /* kSwapWrite */, 3 /* kSwapWrite */)
      ],
      true /* smooth */);
  // Geom objects and size.
  memoryBands.addChartSources(
      [new Events(
          model.system.memory, 5 /* kGemObjects */, 5 /* kGemObjects */)],
      true /* smooth */);
  memoryBands.addChartSources(
      [new Events(model.system.memory, 6 /* kGemSize */, 6 /* kGemSize */)],
      true /* smooth */);
  memoryBands.addChartSources(
      [new Events(
          model.system.memory, 12 /* kMemoryPower */, 12 /* kMemoryPower */)],
      true /* smooth */);
  memoryBands.setVSync(vsyncEvents);

  var chromeTitle =
      new EventBandTitle(parent, 'Chrome graphics', 'arc-events-band-title');
  var chromeBands =
      new EventBands(chromeTitle, 'arc-events-band', resolution, 0, duration);
  chromeBands.setWidth(chromeBands.timestampToOffset(duration));
  for (var i = 0; i < model.chrome.buffers.length; i++) {
    chromeBands.addBand(
        new Events(model.chrome.buffers[i], 500, 599), topBandHeight,
        topBandPadding);
  }

  chromeBands.setVSync(vsyncEvents);
  var chromeJanks = new Events(
      model.chrome.global_events, 505 /* kChromeOSJank */,
      505 /* kChromeOSJank */);
  chromeBands.addGlobal(chromeJanks);

  chromeBands.addChartToExistingArea(0 /* top */, chromeBands.height);
  chromeBands.addChartSources(
      [new Events(
          model.system.memory, 7 /* kGpuFrequency */, 7 /* kGpuFrequency */)],
      false /* smooth */);
  chromeBands.addChartSources(
      [new Events(model.system.memory, 11 /* kGpuPower */, 11 /* kGpuPower */)],
      true /* smooth */);


  var androidTitle =
      new EventBandTitle(parent, 'Android graphics', 'arc-events-band-title');
  var androidBands =
      new EventBands(androidTitle, 'arc-events-band', resolution, 0, duration);
  androidBands.setWidth(androidBands.timestampToOffset(duration));
  androidBands.addBand(
      new Events(model.android.buffers[0], 400, 499), topBandHeight,
      topBandPadding);
  // Add vsync events
  androidBands.setVSync(vsyncEvents);
  // Add vsync handler events
  var androidVsyncHandling = new Events(
      model.android.global_events, 400 /* kSurfaceFlingerVsyncHandler */,
      400 /* kSurfaceFlingerVsyncHandler */);
  androidBands.addGlobal(androidVsyncHandling);
  // Add jank events
  var androidJanks = new Events(
      model.android.global_events, 405 /* kSurfaceFlingerCompositionJank */,
      405 /* kSurfaceFlingerCompositionJank */);
  androidBands.addGlobal(androidJanks);

  var allActivityJanks = [];
  var allActivityCustomEvents = [];
  for (var i = 0; i < model.views.length; i++) {
    var view = model.views[i];
    var activityTitleText;
    var icon;
    if (model.tasks && view.task_id in model.tasks) {
      activityTitleText =
          model.tasks[view.task_id].title + ' - ' + view.activity;
      icon = model.tasks[view.task_id].icon;
    } else {
      activityTitleText = 'Task #' + view.task_id + ' - ' + view.activity;
    }
    var activityTitle = new EventBandTitle(
        parent, activityTitleText, 'arc-events-band-title', icon);
    var activityBands = new EventBands(
        activityTitle, 'arc-events-band', resolution, 0, duration);
    activityBands.setWidth(activityBands.timestampToOffset(duration));
    for (var j = 0; j < view.buffers.length; j++) {
      // Android buffer events.
      activityBands.addBand(
          new Events(view.buffers[j], 100, 199), innerBandHeight,
          innerBandPadding);
      // exo events.
      activityBands.addBand(
          new Events(view.buffers[j], 200, 299), innerBandHeight,
          innerBandPadding /* padding */);
      // Chrome buffer events are not displayed at this time.

      // Add separator between buffers.
      if (j != view.buffers.length - 1) {
        activityBands.addBandSeparator(innerBandPadding);
      }
    }
    // Add vsync events
    activityBands.setVSync(vsyncEvents);

    var activityJank = new Events(
        view.global_events, 106 /* kBufferFillJank */,
        106 /* kBufferFillJank */);
    activityBands.addGlobal(activityJank);
    allActivityJanks.push(activityJank);

    var activityCustomEvents = new Events(
        view.global_events, 600 /* kCustomEvent */, 600 /* kCustomEvent */);
    activityBands.addGlobal(activityCustomEvents);
    allActivityCustomEvents.push(activityCustomEvents);
  }

  // Input section if exists.
  if (model.input && model.input.buffers.length > 0) {
    var inputTitle =
        new EventBandTitle(parent, 'Input', 'arc-events-band-title');
    var inputBands =
        new EventBands(inputTitle, 'arc-events-band', resolution, 0, duration);
    inputBands.setWidth(inputBands.timestampToOffset(duration));
    for (var i = 0; i < model.input.buffers.length; i++) {
      inputBands.addBand(
          new Events(model.input.buffers[i], 700, 799), topBandHeight,
          topBandPadding);
    }
    inputBands.setVSync(vsyncEvents);
  }

  // Create time ruler.
  var timeRulerEventHeight = 16;
  var timeRulerLabelHeight = 92;
  var timeRulerTitle =
      new EventBandTitle(parent, '' /* title */, 'arc-time-ruler-title');
  var timeRulerBands = new EventBands(
      timeRulerTitle, 'arc-events-band', resolution, 0, duration);
  timeRulerBands.setWidth(timeRulerBands.timestampToOffset(duration));
  // Reseve space for ticks and global events.
  timeRulerBands.updateHeight(timeRulerEventHeight, 0 /* padding */);

  var kTimeMark = 10000;
  var kTimeMarkSmall = 10001;
  var timeEvents = [];
  var timeTick = 0;
  var timeTickOffset = 20 * resolution;
  var timeTickIndex = 0;
  while (timeTick < duration) {
    if ((timeTickIndex % 10) == 0) {
      timeEvents.push([kTimeMark, timeTick]);
    } else {
      timeEvents.push([kTimeMarkSmall, timeTick]);
    }
    timeTick += timeTickOffset;
    ++timeTickIndex;
  }
  var timeMarkEvents = new Events(timeEvents, kTimeMark, kTimeMarkSmall);
  timeRulerBands.addGlobal(timeMarkEvents);

  // Add all janks
  timeRulerBands.addGlobal(chromeJanks, 'circle' /* renderType */);
  timeRulerBands.addGlobal(androidJanks, 'circle' /* renderType */);
  for (var i = 0; i < allActivityJanks.length; ++i) {
    timeRulerBands.addGlobal(allActivityJanks[i], 'circle' /* renderType */);
  }
  for (var i = 0; i < allActivityCustomEvents.length; ++i) {
    timeRulerBands.addGlobal(
        allActivityCustomEvents[i], 'circle' /* renderType */);
  }
  // Add vsync events
  timeRulerBands.setVSync(vsyncEvents);

  // Reseve space for labels.
  // Add tick labels.
  timeRulerBands.updateHeight(timeRulerLabelHeight, 0 /* padding */);
  timeTick = 0;
  timeTickOffset = 200 * resolution;
  while (timeTick < duration) {
    SVG.addText(
        timeRulerBands.svg, timeRulerBands.timestampToOffset(timeTick),
        timeRulerEventHeight, timeRulerBands.fontSize,
        timestampToMsText(timeTick));
    timeTick += timeTickOffset;
  }
  // Add janks and custom events labels.
  var rotationY = timeRulerEventHeight + timeRulerBands.fontSize;
  for (var i = 0; i < timeRulerBands.globalEvents.length; ++i) {
    var globalEvents = timeRulerBands.globalEvents[i];
    if (globalEvents == timeMarkEvents ||
        globalEvents == timeRulerBands.vsyncEvents) {
      continue;
    }
    var index = globalEvents.getFirstEvent();
    while (index >= 0) {
      var event = globalEvents.events[index];
      index = globalEvents.getNextEvent(index, 1 /* direction */);
      var eventType = event[0];
      var attributes = eventAttributes[eventType];
      var text;
      if (eventType == 600 /* kCustomEvent */) {
        text = event[2];
      } else {
        text = attributes.name;
      }
      var x =
          timeRulerBands.timestampToOffset(event[1]) - timeRulerBands.fontSize;
      SVG.addText(
          timeRulerBands.svg, x, timeRulerEventHeight, timeRulerBands.fontSize,
          text, 'start' /* anchor */,
          'rotate(45 ' + x + ', ' + rotationY + ')' /* transform */);
    }
  }

  $('arc-graphics-tracing-save').disabled = false;
}
