// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DataSeries} from '../model/data_series.js';
import type {HealthdInternalsSystemTrendElement} from '../view/pages/system_trend.js';

import {UnitLabel} from './unit_label.js';

/**
 * The ID of category for system trend.
 */
export enum CategoryTypeEnum {
  BATTERY = 'Battery',
  CPU_FREQUENCY = 'CPU Frequency',
  CPU_USAGE = 'CPU Usage',
  MEMORY = 'Memory',
  THERMAL = 'Thermals',
  ZRAM = 'Zram',
  CUSTOM = 'Custom',
}

/**
 * The data series shared the same label and displayed with the same scale.
 */
export interface DataSeriesList {
  // The data.
  dataList: DataSeries[];
  // Indices of selected data for custom category
  selectedIndices: number[];
  // The helper class to decide displayed unit shared by all data series.
  readonly unitLabel: UnitLabel;
}

export interface DataSeriesCollection {
  battery: DataSeriesList;
  cpuFrequency: DataSeriesList;
  cpuUsage: DataSeriesList;
  memory: DataSeriesList;
  thermal: DataSeriesList;
  zram: DataSeriesList;
}

/**
 * Controller for system trend page. Used to maintain the data and return the
 * data displayed in the line chart.
 */
export class SystemTrendController {
  constructor(element: HealthdInternalsSystemTrendElement) {
    this.element = element;
    this.dataCollection = {
      battery: {
        dataList: [],
        selectedIndices: [],
        unitLabel: new UnitLabel([''], 1),
      },
      cpuFrequency: {
        dataList: [],
        selectedIndices: [],
        unitLabel: new UnitLabel(['kHz', 'MHz', 'GHz'], 1000),
      },
      cpuUsage: {
        dataList: [],
        selectedIndices: [],
        unitLabel: new UnitLabel(['%'], 1),
      },
      memory: {
        dataList: [],
        selectedIndices: [],
        unitLabel: new UnitLabel(['KB', 'MB', 'GB'], 1024),
      },
      thermal: {
        dataList: [],
        selectedIndices: [],
        unitLabel: new UnitLabel(['°C'], 1),
      },
      zram: {
        dataList: [],
        selectedIndices: [],
        unitLabel: new UnitLabel(['B', 'KB', 'MB', 'GB'], 1024),
      },
    };
  }

  // The corresponding Polymer element.
  private element: HealthdInternalsSystemTrendElement;

  // The data for displaying line chart.
  private dataCollection: DataSeriesCollection;

  setBatteryData(dataSeriesList: DataSeries[]) {
    this.dataCollection.battery.dataList = dataSeriesList;
    this.element.refreshData(CategoryTypeEnum.BATTERY);
  }

  setCpuFrequencyData(dataSeriesList: DataSeries[]) {
    this.dataCollection.cpuFrequency.dataList = dataSeriesList;
    if (dataSeriesList.length > 1) {
      // Select the first one and last one to include both big and little cores.
      this.dataCollection.cpuFrequency.selectedIndices =
          [0, dataSeriesList.length - 1];
    }
    this.element.refreshData(CategoryTypeEnum.CPU_FREQUENCY);
  }

  setCpuUsageData(dataSeriesList: DataSeries[]) {
    this.dataCollection.cpuUsage.dataList = dataSeriesList;
    // The first one is overall usage.
    this.dataCollection.cpuUsage.selectedIndices = [0];
    this.element.refreshData(CategoryTypeEnum.CPU_USAGE);
  }

  setMemoryData(dataSeriesList: DataSeries[]) {
    this.dataCollection.memory.dataList = dataSeriesList;
    // The first one is available memory.
    this.dataCollection.memory.selectedIndices = [0];
    this.element.refreshData(CategoryTypeEnum.MEMORY);
  }

  setThermalData(dataSeriesList: DataSeries[]) {
    this.dataCollection.thermal.dataList = dataSeriesList;
    this.element.refreshData(CategoryTypeEnum.THERMAL);
  }

  setZramData(dataSeriesList: DataSeries[]) {
    this.dataCollection.zram.dataList = dataSeriesList;
    // The first one is total used zram.
    this.dataCollection.zram.selectedIndices = [0];
    this.element.refreshData(CategoryTypeEnum.ZRAM);
  }

  setSelectedIndices(type: CategoryTypeEnum, selectedIndices: number[]) {
    switch (type) {
      case CategoryTypeEnum.BATTERY:
        this.dataCollection.battery.selectedIndices = selectedIndices;
        break;
      case CategoryTypeEnum.CPU_FREQUENCY:
        this.dataCollection.cpuFrequency.selectedIndices = selectedIndices;
        break;
      case CategoryTypeEnum.CPU_USAGE:
        this.dataCollection.cpuUsage.selectedIndices = selectedIndices;
        break;
      case CategoryTypeEnum.MEMORY:
        this.dataCollection.memory.selectedIndices = selectedIndices;
        break;
      case CategoryTypeEnum.THERMAL:
        this.dataCollection.thermal.selectedIndices = selectedIndices;
        break;
      case CategoryTypeEnum.ZRAM:
        this.dataCollection.zram.selectedIndices = selectedIndices;
        break;
      case CategoryTypeEnum.CUSTOM:
        console.error('SystemTrendController: Got unexpected type.');
        break;
    }
  }

  /**
   * Get the required data for line chart.
   *
   * @param type - Type of displayed category.
   * @returns - List of `DataSeriesList` data. Except for custom category, we
   *            only return one element in the list for single source.
   */
  getData(type: CategoryTypeEnum): DataSeriesList[] {
    switch (type) {
      case CategoryTypeEnum.BATTERY:
        return [this.dataCollection.battery];
      case CategoryTypeEnum.CPU_FREQUENCY:
        return [this.dataCollection.cpuFrequency];
      case CategoryTypeEnum.CPU_USAGE:
        return [this.dataCollection.cpuUsage];
      case CategoryTypeEnum.MEMORY:
        return [this.dataCollection.memory];
      case CategoryTypeEnum.THERMAL:
        return [this.dataCollection.thermal];
      case CategoryTypeEnum.ZRAM:
        return [this.dataCollection.zram];
      case CategoryTypeEnum.CUSTOM:
        return this.getCustomData();
    }
  }

  private getCustomData(): DataSeriesList[] {
    const output: DataSeriesList[] = [];
    const allData = [
      this.dataCollection.cpuUsage,
      this.dataCollection.cpuFrequency,
      this.dataCollection.memory,
      this.dataCollection.zram,
      this.dataCollection.battery,
      this.dataCollection.thermal,
    ];
    for (const data of allData) {
      if (data.selectedIndices.length === 0) {
        continue;
      }
      output.push({
        dataList: data.selectedIndices.map(i => data.dataList[i]),
        selectedIndices: data.selectedIndices,
        unitLabel: data.unitLabel,
      });
    }
    return output;
  }
}
