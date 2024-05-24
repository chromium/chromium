// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Overview Tracing UI.
 */

/**
 * @type {Array<string>}.
 * List of available colors to be used in charts. Each model is associated with
 * the same color in all charts.
 */
const chartColors = [
  '#e6194B',
  '#3cb44b',
  '#4363d8',
  '#f58231',
  '#911eb4',
  '#42d4f4',
  '#f032e6',
  '#469990',
  '#9A6324',
  '#800000',
  '#808000',
  '#000075',
];

/**
 * @type {Array<Object>}.
 * Array of models to display.
 */
const models = [];

/**
 * @type {Array<string>}.
 * Array of taken colors and it is used to prevent several models are displayed
 * in the same color.
 */
const takenColors = [];

/**
 * Maps model to the associated color.
 */
const modelColors = new Map();

/**
 * Frame time based on 60 FPS.
 */
const targetFrameTime = 16667;

function initializeOverviewUi() {
  initializeUi(8 /* zoomLevel */, function() {
    // Update function.
    refreshModels();
  });
}

/**
 * Helper that calculates overall frequency of events.
 *
 * TODO(matvore): Use |information.(app|perceived)_fps| and delete this function
 * once https://crrev.com/c/5554144 has been around for a while.
 *
 * @param {Events} events events to analyze.
 * @param {number} duration duration of analyzed period.
 */
function calculateFPS(events, duration) {
  let eventCount = 0;
  let index = events.getFirstEvent();
  while (index >= 0) {
    ++eventCount;
    index = events.getNextEvent(index, 1 /* direction */);
  }
  // Duration in micro-seconds.
  return eventCount * 1000000 / duration;
}

/**
 * Helper that calculates render quality and commit deviation. This follows the
 * calculation in |ArcAppPerformanceTracingSession|.
 *
 * @param {Object} model model to process.
 */
function calculateAppRenderQualityAndCommitDeviation(model) {
  const deltas = createDeltaEvents(getGraphicsEvents(model, kExoSurfaceCommit));

  let vsyncErrorDeviationAccumulator = 0.0;
  // Frame delta in microseconds.
  for (let i = 0; i < deltas.events.length; i++) {
    const displayFramesPassed =
        Math.round(deltas.events[i][2] / targetFrameTime);
    const vsyncError =
        deltas.events[i][2] - displayFramesPassed * targetFrameTime;
    vsyncErrorDeviationAccumulator += (vsyncError * vsyncError);
  }
  const commitDeviation =
      Math.sqrt(vsyncErrorDeviationAccumulator / deltas.events.length);

  // Sort by time delta.
  deltas.events.sort(function(a, b) {
    return a[2] - b[2];
  });

  if (deltas.events.length < 3) {
    return [
      0.0 /* % */, 0.0, /* ms */
    ];
  }

  // Get 10% and 90% indices.
  const lowerPosition = Math.round(deltas.events.length / 10);
  const upperPosition = deltas.events.length - 1 - lowerPosition;
  const renderQuality =
      deltas.events[lowerPosition][2] / deltas.events[upperPosition][2];

  return [
    renderQuality * 100.0 /* convert to % */,
    commitDeviation * 0.001, /* mcs to ms */
  ];
}

/**
 * Gets model title as an traced app name. If no information is available it
 * returns default name for app.
 *
 * @param {Object} model model to process.
 */
function getModelTitle(model) {
  return model.information.title ? model.information.title : 'Unknown app';
}

/**
 * Creates view that describes particular model. It shows all relevant
 * information and allows remove the model from the view.
 *
 * @param {Object} model model to process.
 */
function addModelHeader(model) {
  const header = $('arc-overview-tracing-model-template').cloneNode(true);
  header.hidden = false;
  const totalPowerElement =
      header.getElementsByClassName('arc-tracing-app-power-total')[0];
  const cpuPowerElement =
      header.getElementsByClassName('arc-tracing-app-power-cpu')[0];
  const gpuPowerElement =
      header.getElementsByClassName('arc-tracing-app-power-gpu')[0];
  const memoryPowerElement =
      header.getElementsByClassName('arc-tracing-app-power-memory')[0];
  totalPowerElement.parentNode.style.display = 'none';

  if (model.information.icon) {
    header.getElementsByClassName('arc-tracing-app-icon')[0].src =
        'data:image/png;base64,' + model.information.icon;
  }
  header.getElementsByClassName('arc-tracing-app-title')[0].textContent =
      getModelTitle(model);
  const date = model.information.timestamp ?
      new Date(model.information.timestamp).toLocaleString() :
      'Unknown date';
  header.getElementsByClassName('arc-tracing-app-date')[0].textContent = date;
  const duration = (model.information.duration * 0.000001).toFixed(2);
  header.getElementsByClassName('arc-tracing-app-duration')[0].textContent =
      duration;
  const platform = model.information.platform ? model.information.platform :
                                                'Unknown platform';
  header.getElementsByClassName('arc-tracing-app-platform')[0].textContent =
      platform;

  function setFPS(cssClass, type) {
    const fps = calculateFPS(
        getGraphicsEvents(model, type), model.information.duration);
    header.getElementsByClassName(cssClass)[0].textContent = fps.toFixed(2);
  }
  setFPS('arc-tracing-app-fps', kExoSurfaceCommit);
  setFPS('arc-tracing-perceived-fps', kChromeOSPresentationDone);

  const renderQualityAndCommitDeviation =
      calculateAppRenderQualityAndCommitDeviation(model);
  header.getElementsByClassName('arc-tracing-app-render-quality')[0]
      .textContent = renderQualityAndCommitDeviation[0].toFixed(1) + '%';
  header.getElementsByClassName('arc-tracing-app-commit-deviation')[0]
      .textContent = renderQualityAndCommitDeviation[1].toFixed(2) + 'ms';

  const cpuPower = getAveragePower(model, 10 /* kCpuPower */);
  const gpuPower = getAveragePower(model, 11 /* kGpuPower */);
  const memoryPower = getAveragePower(model, 12 /* kMemoryPower */);
  if (cpuPower !== -1 && gpuPower !== -1 && memoryPower !== -1) {
    totalPowerElement.parentNode.style.display = 'block';
    totalPowerElement.textContent =
        (cpuPower + gpuPower + memoryPower).toFixed(2);
    cpuPowerElement.textContent = cpuPower.toFixed(2);
    gpuPowerElement.textContent = gpuPower.toFixed(2);
    memoryPowerElement.textContent = memoryPower.toFixed(2);
  }

  // Handler to remove model from the view.
  header.getElementsByClassName('arc-tracing-close-button')[0].onclick =
      function() {
    removeModel(model);
  };

  header.getElementsByClassName('arc-tracing-dot')[0].style.backgroundColor =
      modelColors.get(model);

  $('arc-overview-tracing-models').appendChild(header);
}

/**
 * Helper that extracts graphics events of a given type.
 *
 * @param {object} model source model whose graphics events to filter
 * @param {number} eventType type of event to extract
 */
function getGraphicsEvents(model, eventType) {
  const events = [];
  const extractor = new Events(model.graphics_events, eventType);
  let index = extractor.getFirstEvent();
  while (index >= 0) {
    events.push(extractor.events[index]);
    index = extractor.getNextEvent(index, 1 /* direction */);
  }

  return new Events(events, eventType);
}

/**
 * Helper that analyzes power events of particular type, calculates overall
 * energy consumption and returns average power between first and last event.
 *
 * @param {object} model source model to analyze.
 * @param {number} eventType event type to match particular power counter
 * @returns {number} average power in watts or -1 in case no events found.
 */
function getAveragePower(model, eventType) {
  const events = new Events(model.system.memory, eventType);
  let lastTimestamp = 0;
  let totalEnergy = 0;
  let index = events.getFirstEvent();
  while (index >= 0) {
    const timestamp = events.events[index][1];
    totalEnergy +=
        events.events[index][2] * (timestamp - lastTimestamp) * 0.001;
    lastTimestamp = timestamp;
    index = events.getNextEvent(index, 1 /* direction */);
  }

  if (!lastTimestamp) {
    return -1;
  }

  return totalEnergy / lastTimestamp;
}

/**
 * Creates events as a smoothed event frequency.
 *
 * @param events source events to analyze.
 * @param {number} duration duration to analyze in microseconds.
 * @param {windowSize} window size to smooth values.
 * @param {step} step to generate next results in microseconds.
 */
function createFPSEvents(events, duration, windowSize, step) {
  const fpsEvents = [];
  let timestamp = 0;
  let index = events.getFirstEvent();
  while (timestamp < duration) {
    let windowFromTimestamp = timestamp - windowSize / 2;
    let windowToTimestamp = timestamp + windowSize / 2;
    // Clamp ranges.
    if (windowToTimestamp > duration) {
      windowFromTimestamp = duration - windowSize;
      windowToTimestamp = duration;
    }
    if (windowFromTimestamp < 0) {
      windowFromTimestamp = 0;
      windowToTimestamp = windowSize;
    }
    while (index >= 0 && events.events[index][1] < windowFromTimestamp) {
      index = events.getNextEvent(index, 1 /* direction */);
    }
    let frames = 0;
    let scanIndex = index;
    while (scanIndex >= 0 && events.events[scanIndex][1] < windowToTimestamp) {
      ++frames;
      scanIndex = events.getNextEvent(scanIndex, 1 /* direction */);
    }
    frames = frames * 1000000 / windowSize;
    fpsEvents.push([0 /* type does not matter */, timestamp, frames]);
    timestamp = timestamp + step;
  }

  return new Events(fpsEvents, 0, 0);
}

/**
 * Creates events as a time difference between events.
 *
 * @param events source events to analyze.
 */
function createDeltaEvents(events) {
  const timeEvents = [];
  const timestamp = 0;
  let lastIndex = events.getFirstEvent();
  while (lastIndex >= 0) {
    const index = events.getNextEvent(lastIndex, 1 /* direction */);
    if (index < 0) {
      break;
    }
    const delta = events.events[index][1] - events.events[lastIndex][1];
    timeEvents.push(
        [0 /* type does not mattter */, events.events[index][1], delta]);
    lastIndex = index;
  }

  return new Events(timeEvents, 0, 0);
}

/**
 * Creates view that shows CPU frequency.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 */
function addCPUFrequencyView(parent, resolution, duration) {
  // Range from 0 to 3GHz
  // 50MHz  1 pixel resolution
  const bands = createChart(
      parent, 'CPU Frequency' /* title */, resolution, duration,
      60 /* height */, 5 /* gridLinesCount */);
  const attributesTemplate =
      Object.assign({}, valueAttributes[9 /* kCpuFrequency */]);
  for (i = 0; i < models.length; i++) {
    const attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources(
        [new Events(models[i].system.memory, 9 /* kCpuFrequency */)],
        true /* smooth */, attributes);
  }
}

/**
 * Creates view that shows CPU temperature.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 */
function addCPUTempView(parent, resolution, duration) {
  // Range from 20 to 100 celsius
  // 2 celsius 1 pixel resolution
  const bands = createChart(
      parent, 'CPU Temperature' /* title */, resolution, duration,
      40 /* height */, 3 /* gridLinesCount */);
  const attributesTemplate =
      Object.assign({}, valueAttributes[8 /* kCpuTemperature */]);
  attributesTemplate.minValue = 20000;
  attributesTemplate.maxValue = 100000;
  for (i = 0; i < models.length; i++) {
    const attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources(
        [new Events(models[i].system.memory, 8 /* kCpuTemperature */)],
        true /* smooth */, attributes);
  }
}

/**
 * Creates view that shows GPU frequency.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 */
function addGPUFrequencyView(parent, resolution, duration) {
  // Range from 300MHz to 1GHz
  // 14MHz  1 pixel resolution
  const bands = createChart(
      parent, 'GPU Frequency' /* title */, resolution, duration,
      50 /* height */, 4 /* gridLinesCount */);
  const attributesTemplate =
      Object.assign({}, valueAttributes[7 /* kGpuFrequency */]);
  attributesTemplate.minValue = 300;   // Mhz
  attributesTemplate.maxValue = 1000;  // Mhz
  for (i = 0; i < models.length; i++) {
    const attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources(
        [new Events(models[i].system.memory, 7 /* kGpuFrequency */)],
        false /* smooth */, attributes);
  }
}

/**
 * Creates view that shows FPS based on a sequence of swap or commit events.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 * @param {string} title the title of the view
 * @param {number} eventType type of the event whose rate to track
 */
function addFPSView(parent, resolution, duration, title, eventType) {
  // FPS range from 10 to 70.
  // 1 fps 1 pixel resolution.
  const bands = createChart(
      parent, title, resolution, duration, 60 /* height */,
      5 /* gridLinesCount */);

  const exportFrameTimes = function(event) {
    // To prevent further handling.
    event.stopPropagation();

    let content = '';
    let fileName = '';
    const modelEvents = [];
    for (i = 0; i < models.length; i++) {
      if (i > 0) {
        content += ',';
      }
      content += models[i].information.title;

      const events = getGraphicsEvents(models[i], eventType);
      modelEvents.push(createDeltaEvents(events));
    }
    fileName = content.replace(',', '_') + '_frame_times.csv';
    content += '\n';
    let index = 0;
    while (true) {
      let line = '';
      let dataExists = false;
      for (i = 0; i < models.length; i++) {
        if (i > 0) {
          line += ',';
        }
        if (modelEvents[i].events.length <= index) {
          continue;
        }
        line += (modelEvents[i].events[index][2] * 0.001).toFixed(2);  // In ms.
        dataExists = true;
      }
      if (!dataExists) {
        break;
      }
      content += line;
      content += '\n';
      index++;
    }

    const contentType = 'text/csv';
    const a = document.createElement('a');
    const blob = new Blob([content], {'type': contentType});
    a.href = window.URL.createObjectURL(blob);
    a.download = fileName;
    a.click();
  };

  bands.createTitleInput(
      'button', 'Export frame times', false, exportFrameTimes);

  const attributesTemplate =
      {maxValue: 70, minValue: 10, name: 'fps', scale: 1.0, width: 1.0};
  for (i = 0; i < models.length; i++) {
    const attributes = Object.assign({}, attributesTemplate);
    const events = getGraphicsEvents(models[i], eventType);
    const fpsEvents = createFPSEvents(
        events, duration, 200000 /* windowSize, 0.2s */, targetFrameTime);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources([fpsEvents], true /* smooth */, attributes);
  }
}

/**
 * Creates view that shows FPS histograms based on app and ChromeOS updates.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {HTMLElement} anchor insert point. View will be added after this.
 * @param {boolean} timeBasedView set to true if histograms are frame times
 *                                based. Otherwise historams contain frame
 *                                count.
 * @param {array[number]} eventTypes array of numeric event types corresponding
 *                                   with frame shown events in each histogram
 */
function addFPSHistograms(parent, anchor, timeBasedView, eventTypes) {
  // Define the width of each bar based on number of models in view.
  let barWidth;
  if (models.length === 1) {
    barWidth = 20;
  } else if (models.length === 2) {
    barWidth = 15;
  } else if (models.length === 3) {
    barWidth = 12;
  } else {
    barWidth = 10;
  }

  const titleYOffset = 12;
  const titleXOffset = 5;
  const titleWidth = 40;
  // Centers of baskets. Main points are 60/0.5 60/1.0, 60/1.5 ...
  const basketFPSs = [90, 60, 40, 30, 24, 20, 17, 15, 12, 10, 8, 6, 4, 2];
  // Minimums of baskets set manually to avoid fractional FPS in output.
  const basketMinFPSs = [70, 50, 35, 26, 22, 19, 16, 13, 11, 9, 7, 5, 3, 0];
  const basketGap = 4;
  const barGap = 2;
  const fullBarsWidth = barWidth * models.length + barGap * (models.length - 1);
  const fullBasketsWidth =
      fullBarsWidth * basketFPSs.length + basketGap * (basketFPSs.length - 1);
  const fullSectionWidth = titleXOffset + titleWidth + fullBasketsWidth;

  // Both for App and for ChromeOS view.
  const totalWidth = fullSectionWidth * 2;

  const title = timeBasedView ? 'SPF Histograms' : 'FPS Histograms';
  const bands = createChart(
      parent, title, 1 /* resolution */, totalWidth /* duration */,
      80 /* height */, 5 /* gridLinesCount */, anchor);

  const viewHandler = function(event, timeBasedView) {
    // To prevent further handling.
    event.stopPropagation();
    // TODO (b:238656897): make it more robust.
    // Section consists of 2 elements: header and SVG view.
    parent.removeChild(anchor.nextSibling);
    parent.removeChild(anchor.nextSibling);
    addFPSHistograms(parent, anchor, timeBasedView, eventTypes);
  };

  const viewHandlerTimeBasedView = function(event) {
    viewHandler(event, true);
  };
  const viewHandlerCountView = function(event) {
    viewHandler(event, false);
  };

  bands.createTitleInput(
      'radio', 'Frame count', !timeBasedView, viewHandlerCountView);
  bands.createTitleInput(
      'radio', 'Frame time', timeBasedView, viewHandlerTimeBasedView);

  bands.addChartText('App', titleXOffset, titleYOffset, 'start' /* anchor */);
  bands.addChartText(
      'Perceived', titleXOffset + fullSectionWidth, titleYOffset,
      'start' /* anchor */);

  const fpsBandYOffset = 80;

  for (let t = 0; t < eventTypes.length; ++t) {
    // Create band with FPSs.
    for (let i = 0; i < basketFPSs.length; ++i) {
      const x = fullSectionWidth * t         // Section offset
          + titleXOffset + titleWidth        // Title offset
          + (fullBarsWidth + basketGap) * i  // Bars begin for this basket.
          + fullBarsWidth * 0.5;             // Center of bars.
      bands.addChartText(
          basketFPSs[i].toString(), x, fpsBandYOffset, 'middle' /* anchor */);
    }

    for (let m = 0; m < models.length; ++m) {
      const events = getGraphicsEvents(models[m], eventTypes[t]);
      // Presort deltas between frames. Fastest one goes first.
      const deltas = createDeltaEvents(events);
      if (deltas.events.length === 0) {
        // Nothing to display.
        continue;
      }

      // Fastest frame goes first.
      deltas.events.sort(function(a, b) {
        return a[2] - b[2];  // Delta between frames.
      });

      // Calculate basket values.
      let index = 0;
      let lastIndex = 0;
      let basketIndex = 0;
      // Keep frame count for baskets.
      const basketCountValues = Array(basketFPSs.length).fill(0);
      // Keep total time of frames for basket.
      const basketTimeValues = Array(basketFPSs.length).fill(0);

      // Maximum basket in context of frame count.
      let basketCountValueMax = 0;
      // Maximum basket in context of total frame times.
      let basketTimeValueMax = 0;

      let totalTime = 0;
      let basketTime = 0;

      while (true) {
        if (index < deltas.events.length) {
          const frameTimeSeconds = deltas.events[index][2] * 0.000001;
          const fps = 1.0 / frameTimeSeconds;
          if (fps > basketMinFPSs[basketIndex]) {
            // This frame is in the current basket.
            basketTime += frameTimeSeconds;
            totalTime += frameTimeSeconds;
            ++index;
            continue;
          }
        }

        // Fill up current basket.
        basketCountValues[basketIndex] = index - lastIndex;
        basketTimeValues[basketIndex] = basketTime;

        // Update maximums.
        if (basketCountValues[basketIndex] > basketCountValueMax) {
          basketCountValueMax = basketCountValues[basketIndex];
        }
        if (basketTimeValues[basketIndex] > basketTimeValueMax) {
          basketTimeValueMax = basketTimeValues[basketIndex];
        }

        // Go the next basket or stop if this was last one.
        ++basketIndex;
        if (basketIndex === basketFPSs.length) {
          break;
        }

        // Reset counters for the next basket.
        basketTime = 0;
        lastIndex = index;
      }

      // Create bars
      const maxBarHeight = 60;
      const barYBase = 60;
      const color = modelColors.get(models[m]);

      let barX = fullSectionWidth * t  // Section offset
          + titleXOffset + titleWidth  // Title offset
          + (barWidth + barGap) * m;   // Model offset of first bar.
      for (let b = 0; b < basketFPSs.length; ++b) {
        let tooltip = '';
        let barHeight = 0;
        if (timeBasedView) {
          barHeight = maxBarHeight * basketTimeValues[b] / basketTimeValueMax;
          let basketInfo = '';
          if (b === 0) {
            basketInfo =
                'Frame time <= ' + (1000.0 / basketMinFPSs[b]).toFixed(1) +
                'ms.';
          } else if (b !== basketFPSs.length - 1) {
            basketInfo = 'Frame time range (' +
                (1000.0 / basketMinFPSs[b - 1]).toFixed(1) + '..' +
                (1000.0 / basketMinFPSs[b]).toFixed(1) + '] ms.';
          } else {
            basketInfo = 'Frame time > ' +
                (1000.0 / basketMinFPSs[b - 1]).toFixed(1) + ' ms.';
          }
          const percent = 100.0 * basketTimeValues[b] / totalTime;
          tooltip = basketInfo + '\n' + percent.toFixed(1) + '% (' +
              +basketTimeValues[b].toFixed(1).toString() + ' of ' +
              totalTime.toFixed(1).toString() + ' sec)';
        } else {
          barHeight = maxBarHeight * basketCountValues[b] / basketCountValueMax;
          let basketInfo = '';
          if (b === 0) {
            basketInfo = 'Frame FPS >= ' + basketMinFPSs[b].toString();
          } else {
            basketInfo = 'Frame FPS range (' + basketMinFPSs[b - 1].toString() +
                '..' + basketMinFPSs[b] + '].';
          }
          const percent = 100.0 * basketCountValues[b] / deltas.events.length;
          tooltip = basketInfo + '\n' + percent.toFixed(1) + '% (' +
              +basketCountValues[b].toString() + ' of ' +
              deltas.events.length.toString() + ' frames)';
        }
        bands.addChartBar(
            barX, barYBase - barHeight, barWidth, barHeight, color);
        bands.addChartTooltip(
            barX, barYBase - maxBarHeight, barWidth, maxBarHeight, tooltip,
            190 /* width */, 40 /* height */);
        barX += (fullBarsWidth + basketGap);
      }
    }
  }
}

/**
 * Creates view that shows commit time delta for app or swap time delta for
 * ChromeOS updates.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 * @param {string} title the title of the view
 * @param {number} eventType type of event whose rate to track
 * @param {number} jankEventType type of event indicating a jank (optional)
 */
function addDeltaView(
    parent, resolution, duration, title, eventType, jankEventType) {
  // time range from 0 to 67ms. 66.67ms is for 15 FPS.
  // 1 ms 1 pixel resolution.  Each grid lines correspond 1/120 FPS time update.
  const bands = createChart(
      parent, title, resolution, duration, 67 /* height */,
      7 /* gridLinesCount */);
  const attributesTemplate = {
    maxValue: 67000,  // microseconds
    minValue: 0,
    name: 'ms',
    scale: 1.0 / 1000.0,
    width: 1.0,
  };
  for (i = 0; i < models.length; i++) {
    const attributes = Object.assign({}, attributesTemplate);
    const events = getGraphicsEvents(models[i], eventType);
    const timeEvents = createDeltaEvents(events);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources([timeEvents], false /* smooth */, attributes);
    if (jankEventType) {
      // Offset each model's janks at a different y position, avoiding max and
      // min positions (0 or 1), as these are awkward when the models are few.
      const y = (i + 1) / (models.length + 1);
      bands.addGlobal(
          getGraphicsEvents(models[i], jankEventType), 'circle',
          attributes.color, y);
    }
  }
}

/**
 * TODO(b/182801299): kernel support was removed for non-root process.
 * Not using feature for now to prevent confusing users.
 *
 * Creates power view for the particular counter.
 *
 * @param {HTMLElement} parent container for the newly created chart.
 * @param {string} title of the chart.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 * @param {number} eventType event type to match particular power counter.
 */
function addPowerView(parent, title, resolution, duration, eventType) {
  let bands = null;
  const attributesTemplate = {
    maxValue: 10000,
    minValue: 0,
    name: 'watts',
    scale: 1.0 / 1000,
    width: 1.0,
  };
  for (i = 0; i < models.length; i++) {
    const events = new Events(models[i].system.memory, eventType, eventType);
    if (events.getFirstEvent() < 0) {
      continue;
    }
    if (bands === null) {
      // power range from 0 to 10000 milli-watts.
      // 200 milli-watts 1 pixel resolution. Each grid line 2 watts
      bands = createChart(
          parent, title, resolution, duration, 50 /* height */,
          4 /* gridLinesCount */);
    }
    const attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources([events], false /* smooth */, attributes);
  }
}

/**
 * Refreshes view, remove all content and creates new one from all available
 * models.
 */
function refreshModels() {
  // Clear previous content.
  $('arc-event-bands').textContent = '';
  $('arc-overview-tracing-models').textContent = '';

  if (models.length === 0) {
    return;
  }

  // Microseconds per pixel. 100% zoom corresponds to 100 mcs per pixel.
  const resolution = zooms[zoomLevel];
  const parent = $('arc-event-bands');

  let duration = 0;
  for (i = 0; i < models.length; i++) {
    duration = Math.max(duration, models[i].information.duration);
  }

  for (i = 0; i < models.length; i++) {
    addModelHeader(models[i]);
  }

  addCPUFrequencyView(parent, resolution, duration);
  addCPUTempView(parent, resolution, duration);
  addGPUFrequencyView(parent, resolution, duration);

  addFPSView(parent, resolution, duration, 'App FPS', kExoSurfaceCommit);
  addDeltaView(
      parent, resolution, duration, 'App commit time', kExoSurfaceCommit,
      kExoSurfaceCommitJank);
  addDeltaView(
      parent, resolution, duration, 'ChromeOS swap time', kChromeOSSwapDone,
      kChromeOSSwapJank);
  addFPSView(
      parent, resolution, duration, 'Perceived FPS', kChromeOSPresentationDone);
  addDeltaView(
      parent, resolution, duration, 'Perceived swap time',
      kChromeOSPresentationDone, kChromeOSPerceivedJank);

  addFPSHistograms(
      parent, parent.lastChild, false /* timeBasedView */,
      [kExoSurfaceCommit, kChromeOSPresentationDone]);
  addPowerView(
      parent, 'Package power constraint', resolution, duration,
      13 /* eventType */);
  addPowerView(parent, 'CPU Power', resolution, duration, 10 /* eventType */);
  addPowerView(parent, 'GPU Power', resolution, duration, 11 /* eventType */);
  addPowerView(
      parent, 'Memory Power', resolution, duration, 12 /* eventType */);
}

/**
 * Assigns color for the model. Tries to be persistent in different runs. It
 * uses timestamp as a source for hash that points to the ideal color. If that
 * color is already taken for another chart, it scans all possible colors and
 * selects the first available. If nothing helps, pink color as assigned as a
 * fallback.
 *
 * @param model model to assign color to.
 */
function setModelColor(model) {
  // Try to assing color bound to timestamp.
  if (model.information && model.information.timestamp) {
    const color = chartColors[
        Math.trunc(model.information.timestamp * 0.001) % chartColors.length];
    if (!takenColors.includes(color)) {
      modelColors.set(model, color);
      takenColors.push(color);
      return;
    }
  }
  // Just find avaiable.
  for (let i = 0; i < chartColors.length; i++) {
    if (!takenColors.includes(chartColors[i])) {
      modelColors.set(model, chartColors[i]);
      takenColors.push(chartColors[i]);
      return;
    }
  }

  // Nothing helps.
  modelColors.set(model, '#ffc0cb');
}

/**
 * Adds model to the view and refreshes everything.
 *
 * @param {object} model to add.
 */
function addModel(model) {
  models.push(model);

  setModelColor(model);
  const graphicsEvents = [];
  function mergeEvents(es) {
    graphicsEvents.push(...es);
  }

  mergeEvents(model.chrome.global_events);
  model.chrome.buffers.forEach(mergeEvents);
  delete model.chrome;

  model.views.forEach(function(v) {
    mergeEvents(v.global_events);
    v.buffers.forEach(mergeEvents);
  });
  delete model.views;

  // Sort by timestamp.
  graphicsEvents.sort(function(a, b) {
    return a[1] - b[1];
  });
  model.graphics_events = graphicsEvents;

  refreshModels();
}

/**
 * Removes model from the view and refreshes everything.
 *
 * @param {object} model to add.
 */
function removeModel(model) {
  let index = models.indexOf(model);
  if (index === -1) {
    return;
  }

  models.splice(index, 1);

  index = takenColors.indexOf(modelColors.get(model));
  if (index !== -1) {
    takenColors.splice(index, 1);
  }

  modelColors.delete(model);

  refreshModels();
}
