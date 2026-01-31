// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The commands that can be sent to the offscreen document from the service
 * worker.
 */
export enum OffscreenCommand {
  DECODE_AUDIO = 'decodeAudio',
}

/**
 * The commands that can be sent to the service worker from the offscreen
 * document.
 */
export enum ServiceWorkerCommand {
  READY = 'ready',
}
