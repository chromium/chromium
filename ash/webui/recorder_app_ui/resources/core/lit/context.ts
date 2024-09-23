// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MicrophoneManager} from '../microphone_manager.js';
import {PlatformHandler} from '../platform_handler.js';
import {signal} from '../reactive/signal.js';
import {RecordingDataManager} from '../recording_data_manager.js';
import {assert, assertExists} from '../utils/assert.js';

interface Context {
  microphoneManager: MicrophoneManager;
  recordingDataManager: RecordingDataManager;
  platformHandler: PlatformHandler;
}

const context = signal<Context|null>(null);

/**
 * Initializes the context with a value.
 *
 * This should only be called once in initialization.
 */
export function initContext(c: Context): void {
  assert(context.value === null, 'Context should only be initialized once');
  context.value = c;
}

/**
 * Returns the current MicrophoneManager in context.
 */
export function useMicrophoneManager(): MicrophoneManager {
  return assertExists(context.value).microphoneManager;
}

/**
 * Returns the current RecordingDataManager in context.
 */
export function useRecordingDataManager(): RecordingDataManager {
  return assertExists(context.value).recordingDataManager;
}

/**
 * Returns the current PlatformHandler in context.
 */
export function usePlatformHandler(): PlatformHandler {
  return assertExists(context.value).platformHandler;
}
