// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used by chrome://healthd-internals.
 */

/**
 * The color set used for drawing line chart.
 */
export const LINE_CHART_COLOR_SET: string[] = [
  '#402812',
  '#892034',
  '#00344d',
  '#004236',
  '#212930',
  '#603618',
  '#7a2257',
  '#003066',
  '#404616',
  '#565f68',
  '#956733',
  '#6f256c',
  '#00527e',
  '#9f9b04',
  '#83888d',
];

/**
 * The enum for displayed pages in chrome://healthd-internals.
 */
export enum PagePath {
  // Only used when menu tabs are not displayed. No page should be displayed.
  NONE = '/',
  TELEMETRY = '/telemetry',
  PROCESS = '/process',
  BATTERY = '/battery',
  CPU_FREQUENCY = '/cpu_frequency',
  CPU_USAGE = '/cpu_usage',
  MEMORY = '/memory',
  THERMAL = '/thermal',
}
