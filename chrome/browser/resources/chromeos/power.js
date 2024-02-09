// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {$} from 'chrome://resources/ash/common/util.js';

const devicePixelRatio = window.devicePixelRatio;

/**
 * Plot a line graph of data versus time on a HTML canvas element.
 *
 * @param {HTMLCanvasElement} plotCanvas The canvas on which the line graph is
 *     drawn.
 * @param {HTMLCanvasElement} legendCanvas The canvas on which the legend for
 *     the line graph is drawn.
 * @param {Array<number>} tData The time (in seconds) in the past when the
 *     corresponding data in plots was sampled.
 * @param {Array<{data: Array<number>, color: string}>} plots An
 *     array of plots to plot on the canvas. The field 'data' of a plot is an
 *     array of samples to be plotted as a line graph with color speficied by
 *     the field 'color'. The elements in the 'data' array are ordered
 *     corresponding to their sampling time in the argument 'tData'. Also, the
 *     number of elements in the 'data' array should be the same as in the time
 *     array 'tData' above.
 * @param {number} yMin Minimum bound of y-axis
 * @param {number} yMax Maximum bound of y-axis.
 * @param {number} yPrecision An integer value representing the number of
 *     digits of precision the y-axis data should be printed with.
 */
function plotLineGraph(
    plotCanvas, legendCanvas, tData, plots, yMin, yMax, yPrecision) {
  const textFont = 12 * devicePixelRatio + 'px Arial';
  const textHeight = 12 * devicePixelRatio;
  const padding = 5 * devicePixelRatio;  // Pixels
  const errorOffsetPixels = 15 * devicePixelRatio;
  const gridColor = '#ccc';
  const plotCtx = plotCanvas.getContext('2d');
  const size = tData.length;

  function drawText(ctx, text, x, y) {
    ctx.font = textFont;
    ctx.fillStyle = '#000';
    ctx.fillText(text, x, y);
  }

  function printErrorText(ctx, text) {
    ctx.clearRect(0, 0, plotCanvas.width, plotCanvas.height);
    drawText(ctx, text, errorOffsetPixels, errorOffsetPixels);
  }

  if (size < 2) {
    printErrorText(
        plotCtx, loadTimeData.getString('notEnoughDataAvailableYet'));
    return;
  }

  for (let count = 0; count < plots.length; count++) {
    if (plots[count].data.length !== size) {
      throw new Error('Mismatch in time and plot data.');
    }
  }

  function valueToString(value) {
    if (Math.abs(value) < 1) {
      return Number(value).toFixed(yPrecision - 1);
    } else {
      return Number(value).toPrecision(yPrecision);
    }
  }

  function getTextWidth(ctx, text) {
    ctx.font = textFont;
    // For now, all text is drawn to the left of vertical lines, or centered.
    // Add a 2 pixel padding so that there is some spacing between the text
    // and the vertical line.
    return Math.round(ctx.measureText(text).width) + 2 * devicePixelRatio;
  }

  function getLegend(text) {
    return ' ' + text + '    ';
  }

  function drawHighlightText(ctx, text, x, y, color) {
    ctx.strokeStyle = '#000';
    ctx.strokeRect(x, y - textHeight, getTextWidth(ctx, text), textHeight);
    ctx.fillStyle = color;
    ctx.fillRect(x, y - textHeight, getTextWidth(ctx, text), textHeight);
    ctx.fillStyle = '#fff';
    ctx.fillText(text, x, y);
  }

  function drawLine(ctx, x1, y1, x2, y2, color) {
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.strokeStyle = color;
    ctx.lineWidth = 1 * devicePixelRatio;
    ctx.stroke();
    ctx.restore();
  }

  // The strokeRect method of the 2d context of a plotCanvas draws a bounding
  // rectangle with an offset origin and greater dimensions. Hence, use this
  // function to draw a rect at the desired location with desired dimensions.
  function drawRect(ctx, x, y, width, height, color) {
    const offset = 1 * devicePixelRatio;
    drawLine(ctx, x, y, x + width - offset, y, color);
    drawLine(ctx, x, y, x, y + height - offset, color);
    drawLine(
        ctx, x, y + height - offset, x + width - offset, y + height - offset,
        color);
    drawLine(
        ctx, x + width - offset, y, x + width - offset, y + height - offset,
        color);
  }

  function drawLegend() {
    // Show a legend only if at least one individual plot has a name.
    let valid = false;
    for (let i = 0; i < plots.length; i++) {
      if (plots[i].name != null) {
        valid = true;
        break;
      }
    }
    if (!valid) {
      legendCanvas.hidden = true;
      return;
    }


    const padding = 2 * devicePixelRatio;
    const legendSquareSide = 12 * devicePixelRatio;
    const legendCtx = legendCanvas.getContext('2d');
    let xLoc = padding;
    let yLoc = padding;
    // Adjust the height of the canvas before drawing on it.
    for (let i = 0; i < plots.length; i++) {
      if (plots[i].name == null) {
        continue;
      }
      const legendText = getLegend(plots[i].name);
      xLoc +=
          legendSquareSide + getTextWidth(legendCtx, legendText) + 2 * padding;
      if (i < plots.length - 1) {
        const xLocNext = xLoc +
            getTextWidth(legendCtx, getLegend(plots[i + 1].name)) +
            legendSquareSide;
        if (xLocNext >= legendCanvas.width) {
          xLoc = padding;
          yLoc = yLoc + 2 * padding + textHeight;
        }
      }
    }

    legendCanvas.height = yLoc + textHeight + padding;
    legendCanvas.style.height = legendCanvas.height / devicePixelRatio + 'px';

    xLoc = padding;
    yLoc = padding;
    // Go over the plots again, this time drawing the legends.
    for (let i = 0; i < plots.length; i++) {
      legendCtx.fillStyle = plots[i].color;
      legendCtx.fillRect(xLoc, yLoc, legendSquareSide, legendSquareSide);
      xLoc += legendSquareSide;

      const legendText = getLegend(plots[i].name);
      drawText(legendCtx, legendText, xLoc, yLoc + textHeight - 1);
      xLoc += getTextWidth(legendCtx, legendText) + 2 * padding;

      if (i < plots.length - 1) {
        const xLocNext = xLoc +
            getTextWidth(legendCtx, getLegend(plots[i + 1].name)) +
            legendSquareSide;
        if (xLocNext >= legendCanvas.width) {
          xLoc = padding;
          yLoc = yLoc + 2 * padding + textHeight;
        }
      }
    }
  }

  const yMinStr = valueToString(yMin);
  const yMaxStr = valueToString(yMax);
  const yHalfStr = valueToString((yMax + yMin) / 2);
  const yMinWidth = getTextWidth(plotCtx, yMinStr);
  const yMaxWidth = getTextWidth(plotCtx, yMaxStr);
  const yHalfWidth = getTextWidth(plotCtx, yHalfStr);

  const xMinStr = tData[0];
  const xMaxStr = tData[size - 1];
  const xMinWidth = getTextWidth(plotCtx, xMinStr);
  const xMaxWidth = getTextWidth(plotCtx, xMaxStr);

  const xOrigin =
      padding + Math.max(yMinWidth, yMaxWidth, Math.round(xMinWidth / 2));
  const yOrigin = padding + textHeight;
  let width = plotCanvas.width - xOrigin - Math.floor(xMaxWidth / 2) - padding;
  if (width < size) {
    plotCanvas.width += size - width;
    width = size;
  }
  const height = plotCanvas.height - yOrigin - textHeight - padding;
  const linePlotEndMarkerWidth = 3;

  function drawPlots() {
    // Start fresh.
    plotCtx.clearRect(0, 0, plotCanvas.width, plotCanvas.height);

    // Draw the bounding rectangle.
    drawRect(plotCtx, xOrigin, yOrigin, width, height, gridColor);

    // Draw the x and y bound values.
    drawText(plotCtx, yMaxStr, xOrigin - yMaxWidth, yOrigin + textHeight);
    drawText(plotCtx, yMinStr, xOrigin - yMinWidth, yOrigin + height);
    drawText(
        plotCtx, xMinStr, xOrigin - xMinWidth / 2,
        yOrigin + height + textHeight);
    drawText(
        plotCtx, xMaxStr, xOrigin + width - xMaxWidth / 2,
        yOrigin + height + textHeight);

    // Draw y-level (horizontal) lines.
    drawLine(
        plotCtx, xOrigin + 1, yOrigin + height / 4, xOrigin + width - 2,
        yOrigin + height / 4, gridColor);
    drawLine(
        plotCtx, xOrigin + 1, yOrigin + height / 2, xOrigin + width - 2,
        yOrigin + height / 2, gridColor);
    drawLine(
        plotCtx, xOrigin + 1, yOrigin + 3 * height / 4, xOrigin + width - 2,
        yOrigin + 3 * height / 4, gridColor);

    // Draw half-level value.
    drawText(
        plotCtx, yHalfStr, xOrigin - yHalfWidth,
        yOrigin + height / 2 + textHeight / 2);

    // Draw the plots.
    const yValRange = yMax - yMin;
    for (let count = 0; count < plots.length; count++) {
      const plot = plots[count];
      const yData = plot.data;
      plotCtx.strokeStyle = plot.color;
      plotCtx.lineWidth = 2;
      plotCtx.beginPath();
      let beginPath = true;
      for (let i = 0; i < size; i++) {
        const val = yData[i];
        if (typeof val === 'string') {
          // Stroke the plot drawn so far and begin a fresh plot.
          plotCtx.stroke();
          plotCtx.beginPath();
          beginPath = true;
          continue;
        }
        const xPos = xOrigin + Math.floor(i / (size - 1) * (width - 1));
        const yPos = yOrigin + height - 1 -
            Math.round((val - yMin) / yValRange * (height - 1));
        if (beginPath) {
          plotCtx.moveTo(xPos, yPos);
          // A simple move to does not print anything. Hence, draw a little
          // square here to mark a beginning.
          plotCtx.fillStyle = '#000';
          plotCtx.fillRect(
              xPos - linePlotEndMarkerWidth, yPos - linePlotEndMarkerWidth,
              linePlotEndMarkerWidth * devicePixelRatio,
              linePlotEndMarkerWidth * devicePixelRatio);
          beginPath = false;
        } else {
          plotCtx.lineTo(xPos, yPos);
          if (i === size - 1 || typeof yData[i + 1] === 'string') {
            // Draw a little square to mark an end to go with the start
            // markers from above.
            plotCtx.fillStyle = '#000';
            plotCtx.fillRect(
                xPos - linePlotEndMarkerWidth, yPos - linePlotEndMarkerWidth,
                linePlotEndMarkerWidth * devicePixelRatio,
                linePlotEndMarkerWidth * devicePixelRatio);
          }
        }
      }
      plotCtx.stroke();
    }

    // Paint the missing time intervals with |gridColor|.
    // Pick one of the plots to look for missing time intervals.
    function drawMissingRect(start, end) {
      const xLeft = xOrigin + Math.floor(start / (size - 1) * (width - 1));
      const xRight = xOrigin + Math.floor(end / (size - 1) * (width - 1));
      plotCtx.fillStyle = gridColor;
      // The x offsets below are present so that the blank space starts
      // and ends between two valid samples.
      plotCtx.fillRect(xLeft + 1, yOrigin, xRight - xLeft - 2, height - 1);
    }
    let inMissingInterval = false;
    let intervalStart;
    for (let i = 0; i < size; i++) {
      if (typeof plots[0].data[i] === 'string') {
        if (!inMissingInterval) {
          inMissingInterval = true;
          // The missing interval should actually start from the previous
          // sample.
          intervalStart = Math.max(i - 1, 0);
        }

        if (i === size - 1) {
          // If this is the last sample, just draw missing rect.
          drawMissingRect(intervalStart, i);
        }
      } else if (inMissingInterval) {
        inMissingInterval = false;
        drawMissingRect(intervalStart, i);
      }
    }
  }

  function drawTimeGuide(tDataIndex) {
    const x = xOrigin + tDataIndex / (size - 1) * (width - 1);
    drawLine(plotCtx, x, yOrigin, x, yOrigin + height - 1, '#000');
    drawText(
        plotCtx, tData[tDataIndex],
        x - getTextWidth(plotCtx, tData[tDataIndex]) / 2, yOrigin - 2);

    for (let count = 0; count < plots.length; count++) {
      const yData = plots[count].data;

      // Draw small black square on the plot where the time guide intersects
      // it.
      const val = yData[tDataIndex];
      let yPos;
      let valStr;
      if (typeof val === 'string') {
        yPos = yOrigin + Math.round(height / 2);
        valStr = val;
      } else {
        yPos = yOrigin + height - 1 -
            Math.round((val - yMin) / (yMax - yMin) * (height - 1));
        valStr = valueToString(val);
      }
      plotCtx.fillStyle = '#000';
      plotCtx.fillRect(x - 2, yPos - 2, 4, 4);

      // Draw the val to right of the intersection.
      let yLoc;
      if (yPos - textHeight / 2 < yOrigin) {
        yLoc = yOrigin + textHeight;
      } else if (yPos + textHeight / 2 >= yPos + height) {
        yLoc = yOrigin + height - 1;
      } else {
        yLoc = yPos + textHeight / 2;
      }
      drawHighlightText(plotCtx, valStr, x + 5, yLoc, plots[count].color);
    }
  }

  function onMouseOverOrMove(event) {
    drawPlots();

    const boundingRect = plotCanvas.getBoundingClientRect();
    const x =
        Math.round((event.clientX - boundingRect.left) * devicePixelRatio);
    const y = Math.round((event.clientY - boundingRect.top) * devicePixelRatio);
    if (x < xOrigin || x >= xOrigin + width || y < yOrigin ||
        y >= yOrigin + height) {
      return;
    }

    if (width === size) {
      drawTimeGuide(Math.round(x - xOrigin));
    } else {
      drawTimeGuide(Math.round((x - xOrigin) / (width - 1) * (size - 1)));
    }
  }

  function onMouseOut(event) {
    drawPlots();
  }

  drawLegend();
  drawPlots();
  plotCanvas.addEventListener('mouseover', onMouseOverOrMove);
  plotCanvas.addEventListener('mousemove', onMouseOverOrMove);
  plotCanvas.addEventListener('mouseout', onMouseOut);
}

const sleepSampleInterval = 30 * 1000;  // in milliseconds.
const sleepText = loadTimeData.getString('systemSuspended');
const invalidDataText = loadTimeData.getString('invalidData');
const offlineText = loadTimeData.getString('offlineText');

const plotColors = [
  'Red',
  'Blue',
  'Green',
  'Gold',
  'CadetBlue',
  'LightCoral',
  'LightSlateGray',
  'Peru',
  'DarkRed',
  'LawnGreen',
  'Tan',
];

/**
 * Add canvases for plotting to |plotsDiv|. For every header in |headerArray|,
 * one canvas for the plot and one for its legend are added.
 *
 * @param {Array<string>} headerArray Headers for the different plots to be
 *     added to |plotsDiv|.
 * @param {HTMLElement} plotsDiv The div element into which the canvases
 *     are added.
 * @return {!Object<{plot: !HTMLCanvasElement, legend: !HTMLCanvasElement}>}
 *   Returns an object with the headers as 'keys'. Each element is an object
 *   containing the legend canvas and the plot canvas that have been added to
 *   |plotsDiv|.
 */
function addCanvases(headerArray, plotsDiv) {
  // Remove the contents before adding new ones.
  while (plotsDiv.firstChild != null) {
    plotsDiv.removeChild(plotsDiv.firstChild);
  }
  const width = Math.floor(plotsDiv.getBoundingClientRect().width);
  const canvases = {};
  for (let i = 0; i < headerArray.length; i++) {
    const header = document.createElement('h4');
    header.textContent = headerArray[i];
    plotsDiv.appendChild(header);

    const legendCanvas =
        /** @type {!HTMLCanvasElement} */ (document.createElement('canvas'));
    legendCanvas.width = width * devicePixelRatio;
    legendCanvas.style.width = width + 'px';
    plotsDiv.appendChild(legendCanvas);

    const plotCanvasDiv = document.createElement('div');
    plotCanvasDiv.style.overflow = 'auto';
    plotsDiv.appendChild(plotCanvasDiv);

    const plotCanvas =
        /** @type {!HTMLCanvasElement} */ (document.createElement('canvas'));
    plotCanvas.width = width * devicePixelRatio;
    plotCanvas.height = 200 * devicePixelRatio;
    plotCanvas.style.height = '200px';
    plotCanvasDiv.appendChild(plotCanvas);

    canvases[headerArray[i]] = {plot: plotCanvas, legend: legendCanvas};
  }
  return canvases;
}

/**
 * Add samples in |sampleArray| to individual plots in |plots|. If the system
 * resumed from a sleep/suspend, then "suspended" sleep samples are added to
 * the plot for the sleep duration.
 *
 * @param {Array<{data: Array<number|string>, color: string}>} plots An
 *     array of plots to plot on the canvas. The field 'data' of a plot is an
 *     array of samples to be plotted as a line graph with color speficied by
 *     the field 'color'. The elements in the 'data' array are ordered
 *     corresponding to their sampling time in the argument 'tData'. Also, the
 *     number of elements in the 'data' array should be the same as in the time
 *     array 'tData' below.
 * @param {Array<string>} tData The time (in seconds) in the past when the
 *     corresponding data in plots was sampled.
 * @param {Array<number>} absTime
 * @param {Array<number>} sampleArray The array of samples wherein each
 *     element corresponds to the individual plot in |plots|.
 * @param {number} sampleTime Time in milliseconds since the epoch when the
 *     samples in |sampleArray| were captured.
 * @param {number} previousSampleTime Time in milliseconds since the epoch
 *     when the sample prior to the current sample was captured.
 * @param {Array<{time: number, sleepDuration: number}>} systemResumedArray An
 *     array objects corresponding to system resume events. The 'time' field is
 *     for the time in milliseconds since the epoch when the system resumed. The
 *     'sleepDuration' field is for the time in milliseconds the system spent
 *     in sleep/suspend state.
 */
function addTimeDataSample(
    plots, tData, absTime, sampleArray, sampleTime, previousSampleTime,
    systemResumedArray) {
  for (let i = 0; i < plots.length; i++) {
    if (plots[i].data.length !== tData.length) {
      throw new Error('Mismatch in time and plot data.');
    }
  }

  let time;
  if (tData.length === 0) {
    time = new Date(sampleTime);
    absTime[0] = sampleTime;
    tData[0] = time.toLocaleTimeString();
    for (let i = 0; i < plots.length; i++) {
      plots[i].data[0] = sampleArray[i];
    }
    return;
  }

  for (let i = 0; i < systemResumedArray.length; i++) {
    const resumeTime = systemResumedArray[i].time;
    const sleepDuration = systemResumedArray[i].sleepDuration;
    let sleepStartTime = resumeTime - sleepDuration;
    if (resumeTime < sampleTime) {
      if (sleepStartTime < previousSampleTime) {
        // This can happen if pending callbacks were handled before actually
        // suspending.
        sleepStartTime = previousSampleTime + 1000;
      }
      // Add sleep samples for every |sleepSampleInterval|.
      let sleepSampleTime = sleepStartTime;
      while (sleepSampleTime < resumeTime) {
        time = new Date(sleepSampleTime);
        absTime.push(sleepSampleTime);
        tData.push(time.toLocaleTimeString());
        for (let j = 0; j < plots.length; j++) {
          plots[j].data.push(sleepText);
        }
        sleepSampleTime += sleepSampleInterval;
      }
    }
  }

  time = new Date(sampleTime);
  absTime.push(sampleTime);
  tData.push(time.toLocaleTimeString());
  for (let i = 0; i < plots.length; i++) {
    plots[i].data.push(sampleArray[i]);
  }
}

/**
 * Display the battery charge vs time on a line graph.
 *
 * powerSupplyData:
 * An array of objects with fields representing the battery charge, time when
 * the charge measurement was taken, and whether there was external power
 * connected at that time.
 *
 * systemResumedData:
 * An array objects with fields 'time' and 'sleepDuration'. Each object
 * corresponds to a system resume event. The 'time' field is for the time in
 * milliseconds since the epoch when the system resumed. The 'sleepDuration'
 * field is for the time in milliseconds the system spent in sleep/suspend
 * state.
 *
 * @param {{
 *   powerSupplyData: Array<{time: number,
 *                           batteryPercent: number,
 *                           batteryDischargeRate: number,
 *                           externalPower: number}>,
 *   systemResumedData: Array<{time: ?, sleepDuration: ?}>
 *  }} destructedParams
 */
function showBatteryChargeData({powerSupplyData, systemResumedData}) {
  const chargeTimeData = [];
  const chargeAbsTime = [];
  const chargePlot = [{
    name: loadTimeData.getString('batteryChargePercentageHeader'),
    color: 'Blue',
    data: [],
  }];
  const dischargeRateTimeData = [];
  const dischargeRateAbsTime = [];
  const dischargeRatePlot = [
    {
      name: loadTimeData.getString('dischargeRateLegendText'),
      color: 'Red',
      data: [],
    },
    {
      name: loadTimeData.getString('movingAverageLegendText'),
      color: 'Green',
      data: [],
    },
    {
      name: loadTimeData.getString('binnedAverageLegendText'),
      color: 'Blue',
      data: [],
    },
  ];
  let minDischargeRate = 1000;   // A high unrealistic number to begin with.
  let maxDischargeRate = -1000;  // A low unrealistic number to begin with.
  for (let i = 0; i < powerSupplyData.length; i++) {
    const j = Math.max(i - 1, 0);

    addTimeDataSample(
        chargePlot, chargeTimeData, chargeAbsTime,
        [powerSupplyData[i].batteryPercent], powerSupplyData[i].time,
        powerSupplyData[j].time, systemResumedData);

    const dischargeRate = powerSupplyData[i].batteryDischargeRate;
    const inputSampleCount = $('sample-count-input').value;

    let movingAverage = 0;
    let k = 0;
    for (k = 0; k < inputSampleCount && i - k >= 0; k++) {
      movingAverage += powerSupplyData[i - k].batteryDischargeRate;
    }
    // |k| will be atleast 1 because the 'min' value of the input field is 1.
    movingAverage /= k;

    let binnedAverage = 0;
    for (k = 0; k < inputSampleCount; k++) {
      const currentSampleIndex = i - i % inputSampleCount + k;
      if (currentSampleIndex >= powerSupplyData.length) {
        break;
      }

      binnedAverage += powerSupplyData[currentSampleIndex].batteryDischargeRate;
    }
    binnedAverage /= k;

    minDischargeRate = Math.min(dischargeRate, minDischargeRate);
    maxDischargeRate = Math.max(dischargeRate, maxDischargeRate);
    addTimeDataSample(
        dischargeRatePlot, dischargeRateTimeData, dischargeRateAbsTime,
        [dischargeRate, movingAverage, binnedAverage], powerSupplyData[i].time,
        powerSupplyData[j].time, systemResumedData);
  }
  if (minDischargeRate === maxDischargeRate) {
    // This means that all the samples had the same value. Hence, offset the
    // extremes by a bit so that the plot looks good.
    minDischargeRate -= 1;
    maxDischargeRate += 1;
  }

  const plotsDiv = $('battery-charge-plots-div');

  const canvases = addCanvases(
      [
        loadTimeData.getString('batteryChargePercentageHeader'),
        loadTimeData.getString('batteryDischargeRateHeader'),
      ],
      plotsDiv);

  const batteryChargeCanvases =
      canvases[loadTimeData.getString('batteryChargePercentageHeader')];
  plotLineGraph(
      batteryChargeCanvases['plot'], batteryChargeCanvases['legend'],
      chargeTimeData, chargePlot, 0.00, 100.00, 3);

  const dischargeRateCanvases =
      canvases[loadTimeData.getString('batteryDischargeRateHeader')];
  plotLineGraph(
      dischargeRateCanvases['plot'], dischargeRateCanvases['legend'],
      dischargeRateTimeData, dischargeRatePlot, minDischargeRate,
      maxDischargeRate, 3);
}

/**
 * Shows state occupancy data (CPU idle or CPU freq state occupancy) on a set of
 * plots on the about:power UI.
 *
 * @param {Array<Array<{
 *     time: number,
 *     cpuOnline: boolean,
 *     timeInState: Object<number>}>>} timeInStateData Array of arrays
 *     where each array corresponds to a CPU on the system. The elements of the
 *     individual arrays contain state occupancy samples.
 * @param {Array<{time: ?, sleepDuration: ?}>} systemResumedArray An array
 *     objects with fields 'time' and 'sleepDuration'. Each object corresponds
 *     to a system resume event. The 'time' field is for the time in
 *     milliseconds since the epoch when the system resumed. The 'sleepDuration'
 *     field is for the time in milliseconds the system spent in sleep/suspend
 *     state.
 * @param {string} i18nHeaderString The header string to be displayed with each
 *     plot. For example, CPU idle data will have its own header format, and CPU
 *     freq data will have its header format.
 * @param {?string} unitString This is the string capturing the unit, if any,
 *     for the different states. Note that this is not the unit of the data
 *     being plotted.
 * @param {string} plotsDivId The div element's ID in which the plots should
 *     be added.
 */
function showStateOccupancyData(
    timeInStateData, systemResumedArray, i18nHeaderString, unitString,
    plotsDivId) {
  const cpuPlots = [];
  let tData;
  let absTime;
  for (let cpu = 0; cpu < timeInStateData.length; cpu++) {
    const cpuData = timeInStateData[cpu];
    if (cpuData.length === 0) {
      cpuPlots[cpu] = {plots: [], tData: []};
      continue;
    }
    tData = [];
    absTime = [];
    // Each element of |plots| is an array of samples, one for each of the CPU
    // states. The number of states is dicovered by looking at the first
    // sample for which the CPU is online.
    const plots = [];
    const stateIndexMap = [];
    let stateCount = 0;
    for (let i = 0; i < cpuData.length; i++) {
      if (cpuData[i].cpuOnline) {
        for (const state in cpuData[i].timeInState) {
          let stateName = state;
          if (unitString != null) {
            stateName += ' ' + unitString;
          }
          plots.push(
              {name: stateName, data: [], color: plotColors[stateCount]});
          stateIndexMap.push(state);
          stateCount += 1;
        }
        break;
      }
    }
    // If stateCount is 0, then it means the CPU has been offline
    // throughout. Just add a single plot for such a case.
    if (stateCount === 0) {
      plots.push({name: null, data: [], color: null});
      stateCount = 1;  // Some invalid state!
    }

    // Pass the samples through the function addTimeDataSample to add 'sleep'
    // samples.
    for (let i = 0; i < cpuData.length; i++) {
      const sample = cpuData[i];
      const valArray = [];
      for (let j = 0; j < stateCount; j++) {
        if (sample.cpuOnline) {
          valArray[j] = sample.timeInState[stateIndexMap[j]];
        } else {
          valArray[j] = offlineText;
        }
      }

      const k = Math.max(i - 1, 0);
      addTimeDataSample(
          plots, tData, absTime, valArray, sample.time, cpuData[k].time,
          systemResumedArray);
    }

    // Calculate the percentage occupancy of each state. A valid number is
    // possible only if two consecutive samples are valid/numbers.
    for (let k = 0; k < stateCount; k++) {
      const stateData = plots[k].data;
      // Skip the first sample as there is no previous sample.
      for (let i = stateData.length - 1; i > 0; i--) {
        if (typeof stateData[i] === 'number') {
          if (typeof stateData[i - 1] === 'number') {
            stateData[i] = (stateData[i] - stateData[i - 1]) /
                (absTime[i] - absTime[i - 1]) * 100;
          } else {
            stateData[i] = invalidDataText;
          }
        }
      }
    }

    // Remove the first sample from the time and data arrays.
    tData.shift();
    for (let k = 0; k < stateCount; k++) {
      plots[k].data.shift();
    }
    cpuPlots[cpu] = {plots: plots, tData: tData};
  }

  const headers = [];
  for (let cpu = 0; cpu < timeInStateData.length; cpu++) {
    headers[cpu] =
        'CPU ' + cpu + ' ' + loadTimeData.getString(i18nHeaderString);
  }

  const canvases = addCanvases(headers, $(plotsDivId));
  for (let cpu = 0; cpu < timeInStateData.length; cpu++) {
    const cpuCanvases = canvases[headers[cpu]];
    plotLineGraph(
        cpuCanvases['plot'], cpuCanvases['legend'], cpuPlots[cpu]['tData'],
        cpuPlots[cpu]['plots'], 0, 100, 3);
  }
}

/**
 * @param {{
 *   idleStateData: Array<Array<{
 *     time: number,
 *     cpuOnline: boolean,
 *     timeInState: Object<number>}>>,
 *   systemResumedData: Array<{time: ?, sleepDuration: ?}>
 * }} destructedParams
 */
function showCpuIdleData({idleStateData, systemResumedData}) {
  showStateOccupancyData(
      idleStateData, systemResumedData, 'idleStateOccupancyPercentageHeader',
      null, 'cpu-idle-plots-div');
}

/**
 * @param {{
 *   freqStateData: Array<Array<{
 *     time: number,
 *     cpuOnline: boolean,
 *     timeInState: Object<number>}>>,
 *   systemResumedData: Array<{time: ?, sleepDuration: ?}>
 * }} destructedParams
 */
function showCpuFreqData({freqStateData, systemResumedData}) {
  showStateOccupancyData(
      freqStateData, systemResumedData,
      'frequencyStateOccupancyPercentageHeader', 'MHz', 'cpu-freq-plots-div');
}

function requestBatteryChargeData() {
  sendWithPromise('requestBatteryChargeData').then(showBatteryChargeData);
}

function requestCpuIdleData() {
  sendWithPromise('requestCpuIdleData').then(showCpuIdleData);
}

function requestCpuFreqData() {
  sendWithPromise('requestCpuFreqData').then(showCpuFreqData);
}

/**
 * Return a callback for the 'Show'/'Hide' buttons for each section of the
 * about:power page.
 *
 * @param {string} sectionId The ID of the section which is to be shown or
 *     hidden.
 * @param {string} buttonId The ID of the 'Show'/'Hide' button.
 * @param {Function} requestFunction The function which should be invoked on
 *    'Show' to request for data from chrome.
 * @return {Function} The button callback function.
 */
function showHideCallback(sectionId, buttonId, requestFunction) {
  return function() {
    if ($(sectionId).hidden) {
      $(sectionId).hidden = false;
      $(buttonId).textContent = loadTimeData.getString('hideButton');
      requestFunction();
    } else {
      $(sectionId).hidden = true;
      $(buttonId).textContent = loadTimeData.getString('showButton');
    }
  };
}

document.addEventListener('DOMContentLoaded', function() {
  $('battery-charge-section').hidden = true;
  $('battery-charge-show-button').onclick = showHideCallback(
      'battery-charge-section', 'battery-charge-show-button',
      requestBatteryChargeData);
  $('battery-charge-reload-button').onclick = requestBatteryChargeData;
  $('sample-count-input').onclick = requestBatteryChargeData;

  $('cpu-idle-section').hidden = true;
  $('cpu-idle-show-button').onclick = showHideCallback(
      'cpu-idle-section', 'cpu-idle-show-button', requestCpuIdleData);
  $('cpu-idle-reload-button').onclick = requestCpuIdleData;

  $('cpu-freq-section').hidden = true;
  $('cpu-freq-show-button').onclick = showHideCallback(
      'cpu-freq-section', 'cpu-freq-show-button', requestCpuFreqData);
  $('cpu-freq-reload-button').onclick = requestCpuFreqData;
});
