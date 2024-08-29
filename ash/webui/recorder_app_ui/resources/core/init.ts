// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataDir} from './data_dir.js';
import {initContext} from './lit/context.js';
import {MicrophoneManager} from './microphone_manager.js';
import {PlatformHandler} from './platform_handler.js';
import {RecordingDataManager} from './recording_data_manager.js';
import {installRouter} from './state/route.js';
import {init as initSettings} from './state/settings.js';

/**
 * Initializes all the systems that need to be ready before the app is mounted.
 */
export async function init(platformHandler: PlatformHandler): Promise<void> {
  let dataDir: DataDir|null = null;

  function handleError(e: unknown) {
    console.error(e);
    platformHandler.handleUncaughtError(e);
  }

  window.addEventListener('unhandledrejection', (rejection) => {
    handleError(rejection.reason);
  });
  window.addEventListener('error', (errorEvent) => {
    handleError(errorEvent.error);
  });

  installRouter();
  initSettings();
  const microphoneManager = await MicrophoneManager.create(
    (deviceId: string) => platformHandler.getMicrophoneInfo(deviceId),
  );
  dataDir = await DataDir.createFromOpfs();
  const recordingDataManager = await RecordingDataManager.create(dataDir);
  initContext({
    microphoneManager,
    recordingDataManager,
    platformHandler,
  });
}
