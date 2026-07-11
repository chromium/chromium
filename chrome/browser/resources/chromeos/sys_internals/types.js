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
 * }} GeneralCpuType
 */

/**
 * For info page.
 * @typedef {{
 *   swapTotal: number,
 *   swapUsed: number,
 *   total: number,
 *   used: number,
 * }} GeneralMemoryType
 */

/**
 * For info page.
 * @typedef {{
 *   compr: number,
 *   comprRatio: number,
 *   orig: number,
 *   total: number,
 * }} GeneralZramType
 */

/**
 * For info page.
 * @typedef {{
 *   usage: number,
 * }} GeneralGpuType
 */

/**
 * For info page.
 * @typedef {{
 *   usage: number,
 * }} GeneralNpuType
 */

/**
 * @typedef {{
 *   cpu: !GeneralCpuType,
 *   memory: !GeneralMemoryType,
 *   zram: !GeneralZramType,
 *   gpu: ?GeneralGpuType,
 *   npu: ?GeneralNpuType,
 * }} GeneralInfoType
 */

/**
 * @typedef {Array<!DataSeries>|null} CpuDataSeriesSet
 */

/**
 * @typedef {{
 *   memUsed: !DataSeries,
 *   swapUsed: !DataSeries,
 *   pswpin: !DataSeries,
 *   pswpout: !DataSeries
 * }} MemoryDataSeriesSet
 */

/**
 * @typedef {{
 *   origDataSize: !DataSeries,
 *   comprDataSize: !DataSeries,
 *   memUsedTotal: !DataSeries,
 *   numReads: !DataSeries,
 *   numWrites: !DataSeries
 * }} ZramDataSeriesSet
 */

/**
 * @typedef {{
 *   cpus: CpuDataSeriesSet,
 *   memory: MemoryDataSeriesSet,
 *   zram: ZramDataSeriesSet,
 * }} DataSeriesSet
 */

/**
 * @typedef {{value: number, timestamp: number}} CounterType
 */
