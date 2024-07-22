// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from '//resources/js/cr.js';

import {LINE_CHART_COLOR_SET} from './constants.js';
import {CpuUsageHelper} from './cpu_usage_helper.js';
import type {CpuUsage} from './cpu_usage_helper.js';
import type {HealthdApiBatteryResult, HealthdApiCpuResult, HealthdApiTelemetryResult, HealthdApiThermalResult} from './externs.js';
import {DataSeries} from './line_chart/utils/data_series.js';
import type {HealthdInternalsBatteryChartElement} from './pages/battery_chart.js';
import type {HealthdInternalsCpuFrequencyChartElement} from './pages/cpu_frequency_chart.js';
import type {HealthdInternalsCpuUsageChartElement} from './pages/cpu_usage_chart.js';
import type {HealthdInternalsTelemetryElement} from './pages/telemetry.js';
import type {HealthdInternalsThermalChartElement} from './pages/thermal_chart.js';

const LINE_CHART_BATTERY_HEADERS: string[] = [
  'Voltage (V)',
  'Charge (Ah)',
  'Current (A)',
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

/**
 * Helper class to collect and maintain the displayed data.
 */
export class DataManager {
  constructor(
      dataRetentionDuration: number,
      telemetryPage: HealthdInternalsTelemetryElement,
      batteryChart: HealthdInternalsBatteryChartElement,
      cpuFrequencyChart: HealthdInternalsCpuFrequencyChartElement,
      cpuUsageChart: HealthdInternalsCpuUsageChartElement,
      thermalChart: HealthdInternalsThermalChartElement) {
    this.dataRetentionDuration = dataRetentionDuration;

    this.telemetryPage = telemetryPage;
    this.batteryChart = batteryChart;
    this.cpuFrequencyChart = cpuFrequencyChart;
    this.cpuUsageChart = cpuUsageChart;
    this.thermalChart = thermalChart;

    this.initBatteryDataSeries();
  }

  // Historical data for line chart. The following `DataSeries` collection
  // is fixed and initialized in constructor.
  // - Battery data.
  private batteryDataSeries: DataSeries[] = [];

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
  private readonly batteryChart: HealthdInternalsBatteryChartElement;
  private readonly cpuFrequencyChart: HealthdInternalsCpuFrequencyChartElement;
  private readonly cpuUsageChart: HealthdInternalsCpuUsageChartElement;
  private readonly thermalChart: HealthdInternalsThermalChartElement;

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
    this.updateThermalData(data.thermals, timestamp);

    const cpuUsage: CpuUsage[][]|null =
        this.cpuUsageHelper.getCpuUsage(data.cpu);
    if (cpuUsage !== null) {
      this.updateCpuUsageData(cpuUsage, timestamp);
    }

    this.removeOutdatedData(timestamp);

    if (data.battery !== undefined) {
      this.batteryChart.updateEndTime(timestamp);
    }
    this.cpuFrequencyChart.updateEndTime(timestamp);
    this.thermalChart.updateEndTime(timestamp);
    if (cpuUsage !== null) {
      this.cpuUsageChart.updateEndTime(timestamp);
    }
  }

  private removeOutdatedData(endTime: number) {
    const newStartTime = endTime - this.dataRetentionDuration;
    for (const dataSeries of this.batteryDataSeries) {
      dataSeries.removeOutdatedData(newStartTime);
    }
    for (const dataSeries of this.cpuFrequencyDataSeries) {
      dataSeries.removeOutdatedData(newStartTime);
    }
    for (const dataSeries of this.thermalDataSeries) {
      dataSeries.removeOutdatedData(newStartTime);
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
    if (cpuNumber !== this.cpuUsageDataSeries.length) {
      console.warn('CPU usage data: Number of CPUs changed.');
      return;
    }

    let count: number = 0;
    for (const logicalCpuUsage of physcialCpuUsage) {
      for (const cpuUsage of logicalCpuUsage) {
        if (cpuUsage.usagePercentage !== null) {
          this.cpuUsageDataSeries[count].addDataPoint(
              cpuUsage.usagePercentage, timestamp);
        }
        count += 1;
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
    this.batteryChart.addDataSeries(this.batteryDataSeries);
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
    this.cpuFrequencyChart.addDataSeries(this.cpuFrequencyDataSeries);
  }

  private initCpuUsageDataSeries(physcialCpuUsage: CpuUsage[][]) {
    let count: number = 0;
    for (const [physicalCpuId, logicalCpuUsage] of physcialCpuUsage.entries()) {
      for (let logicalCpuId: number = 0; logicalCpuId < logicalCpuUsage.length;
           ++logicalCpuId) {
        this.cpuUsageDataSeries.push(new DataSeries(
            `CPU #${physicalCpuId}-${logicalCpuId}`, getLineChartColor(count)));
        count += 1;
      }
    }
    this.cpuUsageChart.addDataSeries(this.cpuUsageDataSeries);
  }

  private initThermalDataSeries(thermals: HealthdApiThermalResult[]) {
    for (const [index, thermal] of thermals.entries()) {
      this.thermalDataSeries.push(new DataSeries(
          `${thermal.name} (${thermal.source})`, getLineChartColor(index)));
    }
    this.thermalChart.addDataSeries(this.thermalDataSeries);
  }
}
