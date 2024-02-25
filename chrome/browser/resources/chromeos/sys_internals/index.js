// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {$} from 'chrome://resources/ash/common/util.js';

import {CPU_COLOR_SET, INFO_PAGE_PRECISION, MEMORY_COLOR_SET, PAGE_HASH, UNITBASE_MEMORY, UNITBASE_NUMBER_PER_SECOND, UNITS_MEMORY, UNITS_NUMBER_PER_SECOND, UPDATE_PERIOD, ZRAM_COLOR_SET} from './constants.js';
import {UnitLabelAlign} from './line_chart/constants.js';
import {DataSeries} from './line_chart/data_series.js';
import {LineChart} from './line_chart/line_chart.js';
import {UnitLabel} from './line_chart/unit_label.js';
import {CounterType, DataSeriesSet, GeneralCpuType, GeneralInfoType, GeneralMemoryType, GeneralZramType, MemoryDataSeriesSet, ZramDataSeriesSet} from './types.js';

/** @type {!DataSeriesSet} */
const dataSeries = initDataSeries();

/** @type {!GeneralInfoType} */
const generalInfo = initGeneralInfo();

/** @type{!Object<string, PromiseResolver>} */
export const promiseResolvers = {
  waitCpuInitialized: new PromiseResolver(),

  /* For testing */
  waitDrawerActionCompleted: null,
  waitOnHashChangeCompleted: null,
  waitSysInternalsInitialized: new PromiseResolver(),
};

/** @type {!LineChart} */
export const lineChart = new LineChart();

/**
 * The maximum value of the counter of the system info data.
 * @type {number}
 */
let counterMax = 0;

/** @const{Map<string, !CounterType>} */
const counterDict = new Map();

/**
 * Initialize the whole page.
 */
export function initialize() {
  initPage();
  initChart();
  startUpdateRequests();

  /* Initialize with the current url hash value. */
  onHashChange();
  promiseResolvers.waitSysInternalsInitialized.resolve();
}

/**
 * Initialize the DOM of the page.
 */
function initPage() {
  $('nav-menu-btn').addEventListener('click', function(event) {
    event.preventDefault();
    openDrawer();
  });

  $('sys-internals-drawer').addEventListener('click', function(event) {
    closeDrawer();
  });

  window.addEventListener('hashchange', onHashChange);
}

/**
 * Open the navbar drawer menu.
 */
export function openDrawer() {
  $('sys-internals-drawer').removeAttribute('hidden');
  /* Preventing CSS transition blocking by JS. */
  setTimeout(function() {
    $('sys-internals-drawer').classList.remove('hidden');
    $('drawer-menu').classList.remove('hidden');
    if (promiseResolvers.waitDrawerActionCompleted) {
      promiseResolvers.waitDrawerActionCompleted.resolve();
    }
  });
}

/**
 * Close the navbar drawer menu.
 */
export function closeDrawer() {
  const /** Element */ drawer = $('sys-internals-drawer');
  drawer.classList.add('hidden');
  $('drawer-menu').classList.add('hidden');
  /* Wait for the drawer close. */
  setTimeout(function() {
    drawer.setAttribute('hidden', '');
    if (promiseResolvers.waitDrawerActionCompleted) {
      promiseResolvers.waitDrawerActionCompleted.resolve();
    }
  }, 200);
}

/**
 * Initialize the data series of the page.
 * @return {!DataSeriesSet}
 */
function initDataSeries() {
  const /** DataSeriesSet */ dataSeriesRes = {
    cpus: null,
    memory: {
      memUsed: new DataSeries('Used Memory', MEMORY_COLOR_SET[0]),
      swapUsed: new DataSeries('Used Swap', MEMORY_COLOR_SET[1]),
      pswpin: new DataSeries('Pswpin', MEMORY_COLOR_SET[2]),
      pswpout: new DataSeries('Pswpout', MEMORY_COLOR_SET[3]),
    },
    zram: {
      origDataSize: new DataSeries('Original Data Size', ZRAM_COLOR_SET[0]),
      comprDataSize: new DataSeries('Compress Data Size', ZRAM_COLOR_SET[1]),
      memUsedTotal: new DataSeries('Total Memory', ZRAM_COLOR_SET[2]),
      numReads: new DataSeries('Num Reads', ZRAM_COLOR_SET[3]),
      numWrites: new DataSeries('Num Writes', ZRAM_COLOR_SET[4]),
    },
  };
  dataSeriesRes.zram.memUsedTotal.setMenuTextBlack(true);
  dataSeriesRes.zram.numReads.setMenuTextBlack(true);
  return dataSeriesRes;
}

/**
 * Initialize generalInfo.
 * @return {!GeneralInfoType}
 */
function initGeneralInfo() {
  return {
    cpu: {
      core: 0,
      idle: 0,
      kernel: 0,
      usage: 0,
      user: 0,
    },
    memory: {
      swapTotal: 0,
      swapUsed: 0,
      total: 0,
      used: 0,
    },
    zram: {
      compr: 0,
      comprRatio: NaN,
      orig: 0,
      total: 0,
    },
  };
}

/**
 * Initialize the LineChart object.
 */
function initChart() {
  lineChart.attachRootDiv($('chart-root'));
}

/**
 * Wait until next period, and then send the update request to backend to
 * update the system information.
 */
function startUpdateRequests() {
  if (window.DONT_SEND_UPDATE_REQUEST) {
    return;
  }

  const doUpdate = function() {
    sendWithPromise('getSysInfo').then(function(data) {
      handleUpdateData(data, Date.now());
    });
  };
  doUpdate();
  setInterval(doUpdate, UPDATE_PERIOD);
}

/**
 * Handle the new data which received from backend.
 * @param {!SysInfoApiResult} data
 * @param {number} timestamp - The time tick for these data.
 */
export function handleUpdateData(data, timestamp) {
  counterMax = data.const.counterMax;

  updateCpuData(data.cpus, timestamp);
  updateMemoryData(data.memory, timestamp);
  updateZramData(data.zram, timestamp);

  if (isInfoPage()) {
    updateInfoPage();
  } else {
    lineChart.updateEndTime(timestamp);
  }
}

/**
 * Handle the new cpu data.
 * @param {!Array<!SysInfoApiCpuResult>} cpus
 * @param {number} timestamp
 */
function updateCpuData(cpus, timestamp) {
  if (dataSeries.cpus == null) {
    initCpuDataSeries(cpus);
  }
  const /** Array<!DataSeries> */ cpuDataSeries = dataSeries.cpus;
  if (cpus.length !== cpuDataSeries.length) {
    console.warn('Cpu Data: Number of processors changed.');
    return;
  }
  let allKernel = 0;
  let allUser = 0;
  let allIdle = 0;
  for (let i = 0; i < cpus.length; ++i) {
    /* Check if this cpu is offline */
    if (cpus[i].total === 0) {
      cpuDataSeries[i].addDataPoint(0, timestamp);
      continue;
    }
    const /** number */ user =
        getDiffAndUpdateCounter(`cpu_${i}_user`, cpus[i].user, timestamp);
    const /** number */ kernel =
        getDiffAndUpdateCounter(`cpu_${i}_kernel`, cpus[i].kernel, timestamp);
    const /** number */ idle =
        getDiffAndUpdateCounter(`cpu_${i}_idle`, cpus[i].idle, timestamp);
    const /** number */ total =
        getDiffAndUpdateCounter(`cpu_${i}_total`, cpus[i].total, timestamp);
    /* Total may be zero at first update. */
    const /** number */ percentage =
        total === 0 ? 0 : (user + kernel) / total * 100;
    cpuDataSeries[i].addDataPoint(percentage, timestamp);
    allKernel += kernel;
    allUser += user;
    allIdle += idle;
  }

  const /** !GeneralCpuType */ generalCpu = generalInfo.cpu;
  generalCpu.core = cpus.length;
  const allTotal = allKernel + allUser + allIdle;
  generalCpu.usage = allTotal === 0 ? 0 : (allKernel + allUser) / allTotal;
  generalCpu.kernel = allTotal === 0 ? 0 : allKernel / allTotal;
  generalCpu.user = allTotal === 0 ? 0 : allUser / allTotal;
  generalCpu.idle = allTotal === 0 ? 0 : allIdle / allTotal;
}

/**
 * Initialize the data series of the page. This function will be called for
 * the first time we get the cpus data.
 * @param {!Array<!SysInfoApiCpuResult>} cpus
 */
function initCpuDataSeries(cpus) {
  if (cpus.length === 0) {
    return;
  }
  dataSeries.cpus = [];
  for (let i = 0; i < cpus.length; ++i) {
    const colorIdx = i % CPU_COLOR_SET.length;
    dataSeries.cpus[i] = new DataSeries(`CPU ${i}`, CPU_COLOR_SET[colorIdx]);
  }
  promiseResolvers.waitCpuInitialized.resolve();
}

/**
 * Handle the new memory data.
 * @param {!SysInfoApiMemoryResult} memory
 * @param {number} timestamp
 */
function updateMemoryData(memory, timestamp) {
  const /** !MemoryDataSeriesSet */ memDataSeries = dataSeries.memory;
  const /** number */ memUsed = memory.total - memory.available;
  memDataSeries.memUsed.addDataPoint(memUsed, timestamp);
  const /** number */ swapUsed = memory.swapTotal - memory.swapFree;
  memDataSeries.swapUsed.addDataPoint(swapUsed, timestamp);
  const /** number */ pswpin =
      getDiffPerSecAndUpdateCounter('pswpin', memory.pswpin, timestamp);
  memDataSeries.pswpin.addDataPoint(pswpin, timestamp);
  const /** number */ pswpout =
      getDiffPerSecAndUpdateCounter('pswpout', memory.pswpout, timestamp);
  memDataSeries.pswpout.addDataPoint(pswpout, timestamp);

  const /** !GeneralMemoryType */ generalMem = generalInfo.memory;
  generalMem.total = memory.total;
  generalMem.used = memUsed;
  generalMem.swapTotal = memory.swapTotal;
  generalMem.swapUsed = swapUsed;
}

/**
 * Handle the new zram data.
 * @param {!SysInfoApiZramResult} zram
 * @param {number} timestamp
 */
function updateZramData(zram, timestamp) {
  const /** !ZramDataSeriesSet */ zramDataSeries = dataSeries.zram;
  zramDataSeries.origDataSize.addDataPoint(zram.origDataSize, timestamp);
  zramDataSeries.comprDataSize.addDataPoint(zram.comprDataSize, timestamp);
  zramDataSeries.memUsedTotal.addDataPoint(zram.memUsedTotal, timestamp);
  const /** number */ numReads =
      getDiffPerSecAndUpdateCounter('numReads', zram.numReads, timestamp);
  zramDataSeries.numReads.addDataPoint(numReads, timestamp);
  const /** number */ numWrites =
      getDiffPerSecAndUpdateCounter('numWrites', zram.numWrites, timestamp);
  zramDataSeries.numWrites.addDataPoint(numWrites, timestamp);

  const /** !GeneralZramType */ generalZram = generalInfo.zram;
  generalZram.total = zram.memUsedTotal;
  generalZram.orig = zram.origDataSize;
  generalZram.compr = zram.comprDataSize;
  /* Note: |comprRatio| may be NaN, this is ok because it can remind user
   * there is no comprRatio now. */
  generalZram.comprRatio =
      (zram.origDataSize - zram.comprDataSize) / zram.origDataSize;
}

/**
 * Get the increments from the last value to the current value. Return the
 * increments, and store the current value.
 * @param {string} name - The key to identify the counter.
 * @param {number} newValue
 * @param {number} timestamp
 * @return {number}
 */
export function getDiffAndUpdateCounter(name, newValue, timestamp) {
  if (counterDict.get(name) === undefined) {
    counterDict.set(name, {value: newValue, timestamp: timestamp});
    return 0;
  }
  const /** !CounterType */ counter = counterDict.get(name);
  let /** number */ valueDelta = newValue - counter.value;

  /* If the increments of the counter is negative, it means that the counter
   * is circulating. Get the real increments with the |counterMax|. The result
   * is guaranteed to be greater than zero because the maximum difference of
   * two counter never greater than |counterMax|. */
  if (valueDelta < 0) {
    valueDelta += counterMax;
  }
  counter.value = newValue;
  counter.timestamp = timestamp;
  return valueDelta;
}

/**
 * Return the average increments per second, and store the current value.
 * @param {string} name - The key to identify the counter.
 * @param {number} newValue
 * @param {number} timestamp
 * @return {number}
 */
export function getDiffPerSecAndUpdateCounter(name, newValue, timestamp) {
  const /** number */ oldTimeStamp =
      counterDict.get(name) ? counterDict.get(name).timestamp : -1;
  const /** number */ valueDelta =
      getDiffAndUpdateCounter(name, newValue, timestamp);

  /* If oldTimeStamp is -1, it means that this is the first value of the
   * counter. */
  if (oldTimeStamp === -1) {
    return 0;
  }

  /**
   * The time increments, in seconds.
   * @type {number}
   */
  const timeDelta = (timestamp - oldTimeStamp) / 1000;
  const /** number */ deltaPerSec =
      (timeDelta === 0) ? 0 : valueDelta / timeDelta;
  return deltaPerSec;
}

/**
 * Updata the info page with the current data.
 */
export function updateInfoPage() {
  const setPercentageById = function(/** string */ id, /** number */ value) {
    setTextById(id, toPercentageString(value, INFO_PAGE_PRECISION));
  };
  const setMemoryById = function(/** string */ id, /** number */ value) {
    setTextById(id, getValueWithUnit(value, UNITS_MEMORY, UNITBASE_MEMORY));
  };

  const /** !GeneralCpuType */ cpu = generalInfo.cpu;
  setTextById('infopage-num-of-cpu', cpu.core.toString());
  setPercentageById('infopage-cpu-usage', cpu.usage);
  setPercentageById('infopage-cpu-kernel', cpu.kernel);
  setPercentageById('infopage-cpu-user', cpu.user);
  setPercentageById('infopage-cpu-idle', cpu.idle);

  const /** !GeneralMemoryType */ memory = generalInfo.memory;
  setMemoryById('infopage-memory-total', memory.total);
  setMemoryById('infopage-memory-used', memory.used);
  setMemoryById('infopage-memory-swap-total', memory.swapTotal);
  setMemoryById('infopage-memory-swap-used', memory.swapUsed);

  const /** !GeneralZramType */ zram = generalInfo.zram;
  setMemoryById('infopage-zram-total', zram.total);
  setMemoryById('infopage-zram-orig', zram.orig);
  setMemoryById('infopage-zram-compr', zram.compr);
  setPercentageById('infopage-zram-compr-ratio', zram.comprRatio);
}

/**
 * Set the element inner text by the element id.
 * @param {string} id
 * @param {string} text
 */
function setTextById(id, text) {
  $(id).innerText = text;
}

/**
 * Transform the number to percentage string.
 * @param {number} number
 * @param {number} fixed - The precision of the number.
 * @return {string}
 */
export function toPercentageString(number, fixed) {
  const fixedNumber = (number * 100).toFixed(fixed);
  return fixedNumber + '%';
}

/**
 * Return the value with a suitable unit. See
 * |getSuitableUint()|.
 * @param {number} value
 * @param {!Array<string>} units
 * @param {number} unitBase
 * @return {string}
 */
export function getValueWithUnit(value, units, unitBase) {
  const result = UnitLabel.getSuitableUnit(value, units, unitBase);
  const suitableValue = result.value.toFixed(INFO_PAGE_PRECISION);
  const unitStr = units[result.unitIdx];
  return `${suitableValue} ${unitStr}`;
}

/**
 * Handle the url onhashchange event.
 */
function onHashChange() {
  const /** string */ hash = location.hash;
  switch (hash) {
    case PAGE_HASH.INFO:
      setupInfoPage();
      break;
    case PAGE_HASH.CPU:
      /* Wait for cpu dataseries initialized. */
      promiseResolvers.waitCpuInitialized.promise.then(setupCPUPage);
      break;
    case PAGE_HASH.MEMORY:
      setupMemoryPage();
      break;
    case PAGE_HASH.ZRAM:
      setupZramPage();
      break;
  }
  const /** Element */ title = $('drawer-title');
  const /** Element */ infoPage = $('infopage-root');
  if (isInfoPage()) {
    title.innerText = 'Info';
    infoPage.removeAttribute('hidden');
  } else {
    title.innerText = hash.slice(1);
    infoPage.setAttribute('hidden', '');
  }

  if (promiseResolvers.waitOnHashChangeCompleted) {
    promiseResolvers.waitOnHashChangeCompleted.resolve();
  }
}

/**
 * Return true if the current page is info page.
 * @return {boolean}
 */
export function isInfoPage() {
  return location.hash === '';
}

/**
 * Set the current page to info page.
 */
function setupInfoPage() {
  lineChart.clearAllSubChart();
  updateInfoPage();
}

const /** number */ LEFT = UnitLabelAlign.LEFT;
const /** number */ RIGHT = UnitLabelAlign.RIGHT;

/**
 * Set the current page to cpu page.
 */
function setupCPUPage() {
  /* This function is async so we need to check the page is still CPU page. */
  if (location.hash !== PAGE_HASH.CPU) {
    return;
  }

  const /** Array<!DataSeries> */ cpuDataSeries = dataSeries.cpus;
  const /** number */ UNITBASE_NO_CARRY = 1;
  const /** !Array<string> */ UNIT_PURE_NUMBER = [''];
  lineChart.setSubChart(LEFT, UNIT_PURE_NUMBER, UNITBASE_NO_CARRY);
  const /** !Array<string> */ UNIT_PERCENTAGE = ['%'];
  lineChart.setSubChart(RIGHT, UNIT_PERCENTAGE, UNITBASE_NO_CARRY);
  lineChart.setSubChartMaxValue(RIGHT, 100);
  for (let i = 0; i < cpuDataSeries.length; ++i) {
    lineChart.addDataSeries(RIGHT, cpuDataSeries[i]);
  }
}

/**
 * Set the current page to memory page.
 */
function setupMemoryPage() {
  const /** !MemoryDataSeriesSet */ memDataSeries = dataSeries.memory;
  lineChart.setSubChart(
      LEFT, UNITS_NUMBER_PER_SECOND, UNITBASE_NUMBER_PER_SECOND);
  lineChart.setSubChart(RIGHT, UNITS_MEMORY, UNITBASE_MEMORY);
  lineChart.addDataSeries(RIGHT, memDataSeries.memUsed);
  lineChart.addDataSeries(RIGHT, memDataSeries.swapUsed);
  lineChart.addDataSeries(LEFT, memDataSeries.pswpin);
  lineChart.addDataSeries(LEFT, memDataSeries.pswpout);
}

/**
 * Set the current page to zram page.
 */
function setupZramPage() {
  const /** !ZramDataSeriesSet */ zramDataSeries = dataSeries.zram;
  lineChart.setSubChart(
      LEFT, UNITS_NUMBER_PER_SECOND, UNITBASE_NUMBER_PER_SECOND);
  lineChart.setSubChart(RIGHT, UNITS_MEMORY, UNITBASE_MEMORY);
  lineChart.addDataSeries(RIGHT, zramDataSeries.origDataSize);
  lineChart.addDataSeries(RIGHT, zramDataSeries.comprDataSize);
  lineChart.addDataSeries(RIGHT, zramDataSeries.memUsedTotal);
  lineChart.addDataSeries(LEFT, zramDataSeries.numReads);
  lineChart.addDataSeries(LEFT, zramDataSeries.numWrites);
}

/* Exposed for testing. */
/*return {
  closeDrawer: closeDrawer,
  dataSeries: dataSeries,
  getDiffAndUpdateCounter: getDiffAndUpdateCounter,
  getDiffPerSecAndUpdateCounter: getDiffPerSecAndUpdateCounter,
  getValueWithUnit: getValueWithUnit,
  handleUpdateData: handleUpdateData,
  initialize: initialize,
  isInfoPage: isInfoPage,
  lineChart: lineChart,
  openDrawer: openDrawer,
  promiseResolvers: promiseResolvers,
  toPercentageString: toPercentageString,
  updateInfoPage: updateInfoPage,
};*/


/** @type {boolean} - Tag used by browser test. */
window.DONT_SEND_UPDATE_REQUEST;
