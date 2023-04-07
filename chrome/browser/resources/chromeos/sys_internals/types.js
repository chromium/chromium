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
export let GeneralCpuType;

/**
 * For info page.
 * @typedef {{
 *   swapTotal: number,
 *   swapUsed: number,
 *   total: number,
 *   used: number,
 * }}
 */
export let GeneralMemoryType;

/**
 * For info page.
 * @typedef {{
 *   compr: number,
 *   comprRatio: number,
 *   orig: number,
 *   total: number,
 * }}
 */
export let GeneralZramType;

/**
 * @typedef {{
 *   cpu: GeneralCpuType,
 *   memory: GeneralMemoryType,
 *   zram: GeneralZramType,
 * }}
 */
export let GeneralInfoType;

/**
 * @typedef {Array<!DataSeries>|null}
 */
export let CpuDataSeriesSet;

/**
 * @typedef {{
 *   memUsed: !DataSeries,
 *   swapUsed: !DataSeries,
 *   pswpin: !DataSeries,
 *   pswpout: !DataSeries
 * }}
 */
export let MemoryDataSeriesSet;

/**
 * @typedef {{
 *   origDataSize: !DataSeries,
 *   comprDataSize: !DataSeries,
 *   memUsedTotal: !DataSeries,
 *   numReads: !DataSeries,
 *   numWrites: !DataSeries
 * }}
 */
export let ZramDataSeriesSet;

/**
 * @typedef {{
 *   cpus: CpuDataSeriesSet,
 *   memory: MemoryDataSeriesSet,
 *   zram: ZramDataSeriesSet,
 * }}
 */
export let DataSeriesSet;

/**
 * @typedef {{value: number, timestamp: number}}
 */
export let CounterType;
