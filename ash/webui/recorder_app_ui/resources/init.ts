// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {init} from './core/init.js';
import {RecorderApp} from './pages/recorder-app.js';
import {PlatformHandler} from './platforms/index.js';

// The error for the promise is handled by the global unhandledrejection
// handler.
// eslint-disable-next-line @typescript-eslint/no-misused-promises
document.addEventListener('DOMContentLoaded', async () => {
  const platformHandler = new PlatformHandler();
  // Initialize core.
  await init(platformHandler);
  // Initialize platform.
  await platformHandler.init();

  document.body.appendChild(new RecorderApp());
});
