// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

export function recordAction(actionName: string) {
  const metricsService = chrome.histograms || chrome.metricsPrivate;
  if (metricsService) {
    metricsService.recordUserAction(actionName);
    metricsService.recordBoolean(actionName, true);
  }
}

export interface UnboundedDialog extends HTMLDialogElement {
  showUnboundedElement?: () => Promise<void>;
  hideUnboundedElement?: () => Promise<void>;
}

export function isGoogleOrigin(origin: string): boolean {
  // <if expr="not is_official_build">
  if (origin === 'http://localhost' || origin.startsWith('http://localhost:') ||
      origin === 'null' || origin.startsWith('file://')) {
    return true;
  }
  // </if>
  // Matches https://*.google.com, https://google.com, https://*.googlers.com,
  // etc. Also matches regional domains like google.co.uk, google.es, etc.
  const googleOriginRegex =
      /^https:\/\/([a-z0-9-]+\.)*(google|googlers)\.(com|[a-z]{2}|co\.[a-z]{2})(\.[a-z]{2})?$/;
  return googleOriginRegex.test(origin);
}

export function getArrayBufferFromBigBuffer(bigBuffer: BigBuffer): ArrayBuffer {
  if (bigBuffer.bytes !== undefined) {
    return new Uint8Array(bigBuffer.bytes).buffer;
  }
  if (bigBuffer.sharedMemory !== undefined) {
    return bigBuffer.sharedMemory.bufferHandle
        .mapBuffer(0, bigBuffer.sharedMemory.size)
        .buffer;
  }
  throw new Error('Invalid BigBuffer');
}

export const HANDSHAKE_INTERVAL_MS = 10;
export const HANDSHAKE_TIMEOUT_MS = 30000;
export const MAX_HANDSHAKE_ATTEMPTS =
    HANDSHAKE_TIMEOUT_MS / HANDSHAKE_INTERVAL_MS;
