// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {sendWithPromise} from '//resources/js/cr.js';

import {LINE_CHART_COLOR_SET} from './constants.js';
import {CpuUsageHelper} from './cpu_usage_helper.js';
import type {CpuUsage} from './cpu_usage_helper.js';
import type {HealthdApiBatteryResult, HealthdApiCpuResult, HealthdApiMemoryResult, HealthdApiTelemetryResult, HealthdApiThermalResult} from './externs.js';
import {DataSeries} from './line_chart/utils/data_series.js';
import type {HealthdInternalsGenericChartElement} from './pages/generic_chart.js';
import type {HealthdInternalsTelemetryElement} from './pages/telemetry.js';

const LINE_CHART_BATTERY_HEADERS: string[] = [
  'Voltage (V)',
  'Charge (Ah)',
  'Current (A)',
];

const LINE_CHART_MEMORY_HEADERS: string[] = [
  'Available',
  'Free',
  'Buffers',
  'Page Cache',
  'Shared',
  'Active',
  'Inactive',
  'Total Slab',
  'Reclaim Slab',
  'Unreclaim Slab',
];

function getLineChartColor(index: number) {
  const colorIdx: number = index % LINE_CHART_COLOR_SET.length;
  return LINE_CHART_COLOR_SET[colorIdx];
}

function sortThermals(
    first: HealthdApiThermalResult, second: HealthdApiThermalResult): number {
  if (first.source === second.source) {
    return first.name.localeCompare(second.name);
  }
  return first.source.localeCompare(second.source);
}

export interface LineChartPages {
  battery: HealthdInternalsGenericChartElement;
  cpuFrequency: HealthdInternalsGenericChartElement;
  cpuUsage: HealthdInternalsGenericChartElement;
  memory: HealthdInternalsGenericChartElement;
  thermal: HealthdInternalsGenericChartElement;
}

/**
 * Helper class to collect and maintain the displayed data.
 */
export class DataManager {
  constructor(
      dataRetentionDuration: number,
      telemetryPage: HealthdInternalsTelemetryElement,
      chartPages: LineChartPages) {
    this.dataRetentionDuration = dataRetentionDuration;

    this.telemetryPage = telemetryPage;
    this.chartPages = chartPages;

    this.initBatteryDataSeries();
    this.initMemoryDataSeries();
  }

  // Historical data for line chart. The following `DataSeries` collection
  // is fixed and initialized in constructor.
  // - Battery data.
  private batteryDataSeries: DataSeries[] = [];
  // - Memory data.
  private memoryDataSeries: DataSeries[] = [];

  // Historical data for line chart. The following `DataSeries` collection
  // is dynamic and initialized when the first batch of data is obtained.
  // - Frequency data of logical CPUs.
  private cpuFrequencyDataSeries: DataSeries[] = [];
  // - CPU usage of logical CPUs in percentage.
  private cpuUsageDataSeries: DataSeries[] = [];
  // - Temperature data of thermal sensors.
  private thermalDataSeries: DataSeries[] = [];

  // Set in constructor.
  private readonly telemetryPage: HealthdInternalsTelemetryElement;
  private readonly chartPages: LineChartPages;

  // The helper class for calculating CPU usage.
  private readonly cpuUsageHelper: CpuUsageHelper = new CpuUsageHelper();

  // The data fetching interval ID used for cancelling the running interval.
  private fetchDataInternalId?: number = undefined;

  // The duration (in milliseconds) that the data will be retained.
  private dataRetentionDuration: number;

  /**
   * Set up periodic fetch data requests to the backend to get required info.
   *
   * @param pollingCycle - Polling cycle in milliseconds.
   */
  setupFetchDataRequests(pollingCycle: number) {
    if (this.fetchDataInternalId !== undefined) {
      clearInterval(this.fetchDataInternalId);
      this.fetchDataInternalId = undefined;
    }
    const fetchData = () => {
      sendWithPromise('getHealthdTelemetryInfo')
          .then((data: HealthdApiTelemetryResult) => {
            this.handleHealthdTelemetryInfo(data);
          });
    };
    fetchData();
    this.fetchDataInternalId = setInterval(fetchData, pollingCycle);
  }

  updateDataRetentionDuration(durationHours: number) {
    this.dataRetentionDuration = durationHours * 60 * 60 * 1000;
    this.removeOutdatedData(Date.now());
  }

  private handleHealthdTelemetryInfo(data: HealthdApiTelemetryResult) {
    data.thermals.sort(sortThermals);

    this.telemetryPage.updateTelemetryData(data);

    const timestamp: number = Date.now();
    if (data.battery !== undefined) {
      this.updateBatteryData(data.battery, timestamp);
    }
    this.updateCpuFrequencyData(data.cpu, timestamp);
    this.updateMemoryData(data.memory, timestamp);
    this.updateThermalData(data.thermals, timestamp);

    const cpuUsage: CpuUsage[][]|null =
        this.cpuUsageHelper.getCpuUsage(data.cpu);
    if (cpuUsage !== null) {
      this.updateCpuUsageData(cpuUsage, timestamp);
    }

    this.removeOutdatedData(timestamp);
  }

  private removeOutdatedData(endTime: number) {
    const newStartTime = endTime - this.dataRetentionDuration;
    const shouldUpdateChart = (dataSeriesList: DataSeries[]) => {
      return dataSeriesList.reduce(
          // The order within `or` expression is important because
          // `removeOutdatedData` should be called for each `dataSeries`.
          (isDataRemoved, dataSeries) =>
              dataSeries.removeOutdatedData(newStartTime) || isDataRemoved,
          false);
    };

    if (shouldUpdateChart(this.batteryDataSeries)) {
      this.chartPages.battery.updateStartTime(newStartTime);
    }
    if (shouldUpdateChart(this.cpuFrequencyDataSeries)) {
      this.chartPages.cpuFrequency.updateStartTime(newStartTime);
    }
    if (shouldUpdateChart(this.cpuUsageDataSeries)) {
      this.chartPages.cpuUsage.updateStartTime(newStartTime);
    }
    if (shouldUpdateChart(this.memoryDataSeries)) {
      this.chartPages.memory.updateStartTime(newStartTime);
    }
    if (shouldUpdateChart(this.thermalDataSeries)) {
      this.chartPages.thermal.updateStartTime(newStartTime);
    }
  }

  private updateBatteryData(
      battery: HealthdApiBatteryResult, timestamp: number) {
    this.batteryDataSeries[0].addDataPoint(battery.voltageNow, timestamp);
    this.batteryDataSeries[1].addDataPoint(battery.chargeNow, timestamp);
    this.batteryDataSeries[2].addDataPoint(battery.currentNow, timestamp);
  }

  private updateCpuFrequencyData(cpu: HealthdApiCpuResult, timestamp: number) {
    if (this.cpuFrequencyDataSeries.length === 0) {
      this.initCpuFrequencyDataSeries(cpu);
    }

    const cpuNumber: number = cpu.physicalCpus.reduce(
        (acc, item) => acc + item.logicalCpus.length, 0);
    if (cpuNumber !== this.cpuFrequencyDataSeries.length) {
      console.warn('CPU frequency data: Number of CPUs changed.');
      return;
    }

    let count: number = 0;
    for (const physicalCpu of cpu.physicalCpus) {
      for (const logicalCpu of physicalCpu.logicalCpus) {
        this.cpuFrequencyDataSeries[count].addDataPoint(
            parseInt(logicalCpu.frequency.current), timestamp);
        count += 1;
      }
    }
  }

  private updateCpuUsageData(
      physcialCpuUsage: CpuUsage[][], timestamp: number) {
    if (this.cpuUsageDataSeries.length === 0) {
      this.initCpuUsageDataSeries(physcialCpuUsage);
    }

    const cpuNumber: number =
        physcialCpuUsage.reduce((acc, item) => acc + item.length, 0);
    if (cpuNumber !== this.cpuUsageDataSeries.length - 1) {
      console.warn('CPU usage data: Number of CPUs changed.');
      return;
    }

    if (cpuNumber === 0) {
      console.warn('CPU usage data: CPU not found.');
      return;
    }

    let sumCpuUsage: number = 0;
    let count: number = 1;
    for (const logicalCpuUsage of physcialCpuUsage) {
      for (const cpuUsage of logicalCpuUsage) {
        if (cpuUsage.usagePercentage !== null) {
          this.cpuUsageDataSeries[count].addDataPoint(
              cpuUsage.usagePercentage, timestamp);
          sumCpuUsage += cpuUsage.usagePercentage;
        }
        count += 1;
      }
    }
    this.cpuUsageDataSeries[0].addDataPoint(sumCpuUsage / cpuNumber, timestamp);
  }

  private updateMemoryData(memory: HealthdApiMemoryResult, timestamp: number) {
    const itemsInChart: Array<string|undefined> = [
      memory.availableMemoryKib,
      memory.freeMemoryKib,
      memory.buffersKib,
      memory.pageCacheKib,
      memory.sharedMemoryKib,
      memory.activeMemoryKib,
      memory.inactiveMemoryKib,
      memory.totalSlabMemoryKib,
      memory.reclaimableSlabMemoryKib,
      memory.unreclaimableSlabMemoryKib,
    ];
    assert(itemsInChart.length === this.memoryDataSeries.length);
    for (const [index, item] of itemsInChart.entries()) {
      if (item !== undefined) {
        this.memoryDataSeries[index].addDataPoint(parseInt(item), timestamp);
      }
    }
  }

  private updateThermalData(
      thermals: HealthdApiThermalResult[], timestamp: number) {
    if (this.thermalDataSeries.length === 0) {
      this.initThermalDataSeries(thermals);
    }

    if (thermals.length !== this.thermalDataSeries.length) {
      console.warn('Thermal data: Number of thermal sensors changed.');
      return;
    }

    for (const [index, thermal] of thermals.entries()) {
      this.thermalDataSeries[index].addDataPoint(
          thermal.temperatureCelsius, timestamp);
    }
  }

  private initBatteryDataSeries() {
    for (const [index, header] of LINE_CHART_BATTERY_HEADERS.entries()) {
      this.batteryDataSeries.push(
          new DataSeries(header, getLineChartColor(index)));
    }
    this.chartPages.battery.addDataSeries(this.batteryDataSeries);
  }

  private initCpuFrequencyDataSeries(cpu: HealthdApiCpuResult) {
    let count: number = 0;
    for (const [physicalCpuId, physicalCpu] of cpu.physicalCpus.entries()) {
      for (let logicalCpuId: number = 0;
           logicalCpuId < physicalCpu.logicalCpus.length; ++logicalCpuId) {
        this.cpuFrequencyDataSeries.push(new DataSeries(
            `CPU #${physicalCpuId}-${logicalCpuId}`, getLineChartColor(count)));
        count += 1;
      }
    }
    this.chartPages.cpuFrequency.addDataSeries(this.cpuFrequencyDataSeries);
  }

  private initCpuUsageDataSeries(physcialCpuUsage: CpuUsage[][]) {
    this.cpuUsageDataSeries.push(
        new DataSeries('Overall', getLineChartColor(0)));
    let count: number = 1;
    for (const [physicalCpuId, logicalCpuUsage] of physcialCpuUsage.entries()) {
      for (let logicalCpuId: number = 0; logicalCpuId < logicalCpuUsage.length;
           ++logicalCpuId) {
        this.cpuUsageDataSeries.push(new DataSeries(
            `CPU #${physicalCpuId}-${logicalCpuId}`, getLineChartColor(count)));
        count += 1;
      }
    }
    this.chartPages.cpuUsage.addDataSeries(this.cpuUsageDataSeries);
  }

  private initMemoryDataSeries() {
    for (const [index, header] of LINE_CHART_MEMORY_HEADERS.entries()) {
      this.memoryDataSeries.push(
          new DataSeries(header, getLineChartColor(index)));
    }
    this.chartPages.memory.addDataSeries(this.memoryDataSeries);
  }

  private initThermalDataSeries(thermals: HealthdApiThermalResult[]) {
    for (const [index, thermal] of thermals.entries()) {
      this.thermalDataSeries.push(new DataSeries(
          `${thermal.name} (${thermal.source})`, getLineChartColor(index)));
    }
    this.chartPages.thermal.addDataSeries(this.thermalDataSeries);
  }
}
