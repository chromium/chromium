// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {init} from './core/init.js';
import {RecorderApp} from './pages/recorder-app.js';
import {PlatformHandler} from './platforms/mojo/handler.js';

document.addEventListener('DOMContentLoaded', () => {
  // TODO(pihsun): Loading state in html before this.
  (async () => {
    const platformHandler = new PlatformHandler();
    // Initialize core.
    await init(platformHandler);
    // Initialize platform.
    platformHandler.init();
    document.body.appendChild(new RecorderApp());
  })().catch((err) => {
    // TODO(pihsun): Handle error state.
    console.error(err);
  });
});
