// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataSeries} from './line_chart/data_series.js';

/**
 * @fileoverview Typedef use by chrome://sys-internals.
 */

/**
 * For info page.
 * @typedef {{
 *   core: number,
 *   idle: number,
 *   kernel: number,
 *   usage: number,
 *   user: number,
 * }}
 */
export var GeneralCpuType;

/**
 * For info page.
 * @typedef {{
 *   swapTotal: number,
 *   swapUsed: number,
 *   total: number,
 *   used: number,
 * }}
 */
export var GeneralMemoryType;

/**
 * For info page.
 * @typedef {{
 *   compr: number,
 *   comprRatio: number,
 *   orig: number,
 *   total: number,
 * }}
 */
export var GeneralZramType;

/**
 * @typedef {{
 *   cpu: GeneralCpuType,
 *   memory: GeneralMemoryType,
 *   zram: GeneralZramType,
 * }}
 */
export var GeneralInfoType;

/**
 * @typedef {Array<!DataSeries>|null}
 */
export var CpuDataSeriesSet;

/**
 * @typedef {{
 *   memUsed: !DataSeries,
 *   swapUsed: !DataSeries,
 *   pswpin: !DataSeries,
 *   pswpout: !DataSeries
 * }}
 */
export var MemoryDataSeriesSet;

/**
 * @typedef {{
 *   origDataSize: !DataSeries,
 *   comprDataSize: !DataSeries,
 *   memUsedTotal: !DataSeries,
 *   numReads: !DataSeries,
 *   numWrites: !DataSeries
 * }}
 */
export var ZramDataSeriesSet;

/**
 * @typedef {{
 *   cpus: CpuDataSeriesSet,
 *   memory: MemoryDataSeriesSet,
 *   zram: ZramDataSeriesSet,
 * }}
 */
export var DataSeriesSet;

/**
 * @typedef {{value: number, timestamp: number}}
 */
export var CounterType;
