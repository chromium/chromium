// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataSeries} from '../model/data_series.js';
import type {HealthdInternalsSystemTrendElement} from '../view/pages/system_trend.js'

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
}

export interface DataSeriesList {
  // The data.
  dataList: DataSeries[];
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
      battery: {dataList: [], unitLabel: new UnitLabel([''], 1)},
      cpuFrequency:
          {dataList: [], unitLabel: new UnitLabel(['kHz', 'MHz', 'GHz'], 1000)},
      cpuUsage: {dataList: [], unitLabel: new UnitLabel(['%'], 1)},
      memory:
          {dataList: [], unitLabel: new UnitLabel(['KiB', 'MiB', 'GiB'], 1024)},
      thermal: {dataList: [], unitLabel: new UnitLabel(['°C'], 1)},
      zram: {
        dataList: [],
        unitLabel: new UnitLabel(['B', 'KiB', 'MiB', 'GiB'], 1024)
      },
    };
  }

  // The corresponding Polymer element.
  private element: HealthdInternalsSystemTrendElement;

  // The data for displaying line chart.
  private dataCollection: DataSeriesCollection;

  setBatteryData(dataSeriesList: DataSeries[]) {
    this.dataCollection.battery.dataList = dataSeriesList;
    this.element.setupDataSeriesList();
  }

  setCpuFrequencyData(dataSeriesList: DataSeries[]) {
    this.dataCollection.cpuFrequency.dataList = dataSeriesList;
    this.element.setupDataSeriesList();
  }

  setCpuUsageData(dataSeriesList: DataSeries[]) {
    this.dataCollection.cpuUsage.dataList = dataSeriesList;
    this.element.setupDataSeriesList();
  }

  setMemoryData(dataSeriesList: DataSeries[]) {
    this.dataCollection.memory.dataList = dataSeriesList;
    this.element.setupDataSeriesList();
  }

  setThermalData(dataSeriesList: DataSeries[]) {
    this.dataCollection.thermal.dataList = dataSeriesList;
    this.element.setupDataSeriesList();
  }

  setZramData(dataSeriesList: DataSeries[]) {
    this.dataCollection.zram.dataList = dataSeriesList;
    this.element.setupDataSeriesList();
  }

  getData(type: CategoryTypeEnum): DataSeriesList {
    switch (type) {
      case CategoryTypeEnum.BATTERY:
        return this.dataCollection.battery;
      case CategoryTypeEnum.CPU_FREQUENCY:
        return this.dataCollection.cpuFrequency;
      case CategoryTypeEnum.CPU_USAGE:
        return this.dataCollection.cpuUsage;
      case CategoryTypeEnum.MEMORY:
        return this.dataCollection.memory;
      case CategoryTypeEnum.THERMAL:
        return this.dataCollection.thermal;
      case CategoryTypeEnum.ZRAM:
        return this.dataCollection.zram;
    }
  }
}
