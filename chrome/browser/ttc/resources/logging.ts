// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

export enum DebugLogTag {
  WEB_SOCKET_MSG = 'WebSocketMsg',
  PAGE_CONTENT = 'PageContent',
  SYSTEM_INSTRUCTION = 'SystemInstruction',
}

/**
 * DEBUG logging tags. By default these log types are all set to be elided.
 * In a local build, set selected logs to true, or pass the command line
 * flag --enable-ttc-debug-logs to enable all debug logs.
 */
const DEBUG_LOG_STATUS: Record<DebugLogTag, boolean> = {
  [DebugLogTag.WEB_SOCKET_MSG]: false,
  [DebugLogTag.PAGE_CONTENT]: false,
  [DebugLogTag.SYSTEM_INSTRUCTION]: false,
};

export function isDebugLogEnabled(debugTag?: DebugLogTag): boolean {
  if (debugTag && DEBUG_LOG_STATUS[debugTag]) {
    return true;
  }
  return loadTimeData.isInitialized() &&
      loadTimeData.valueExists('enableDebugLogs') &&
      loadTimeData.getBoolean('enableDebugLogs');
}

export function log(fileTag: string, msg: string, ...args: any[]) {
  if (!isDebugLogEnabled()) {
    return;
  }
  console.info(
      `\nTTC [${performance.now().toFixed(2)}] [${fileTag}] ${msg}`, ...args);
}

/**
 * Like above but for noisier logs. Includes a tag which can be turned on in a
 * local build to output high volume debugging data.
 */
export function debugLog(
    fileTag: string, debugTag: DebugLogTag, msg: string, ...args: any[]) {
  if (!isDebugLogEnabled(debugTag)) {
    return;
  }
  console.info(
      `\nTTC [${performance.now().toFixed(2)}] [${debugTag}] [${fileTag}] ${
          msg}`,
      ...args);
}

export function warnLog(fileTag: string, msg: string, ...args: any[]) {
  console.warn(
      `\nTTC [${performance.now().toFixed(2)}] [${fileTag}] [WARN] ${msg}`,
      ...args);
}

export function errorLog(fileTag: string, msg: string, ...args: any[]) {
  console.error(
      `\nTTC [${performance.now().toFixed(2)}] [${fileTag}] [ERROR] ${msg}`,
      ...args);
}
