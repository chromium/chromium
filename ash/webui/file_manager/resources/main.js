// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// init_globals.js must be the first loaded module.
import './init_globals.js';
import './strings.m.js';
import 'chrome://file-manager/background/js/metrics_start.js';
import './test_util_swa.js';

import {background} from 'chrome://file-manager/background/js/background.js';
import {VolumeManagerImpl} from 'chrome://file-manager/background/js/volume_manager_impl.js';
import {GlitchType, reportGlitch} from 'chrome://file-manager/common/js/glitch.js';

import {ScriptLoader} from './script_loader.js';

/**
 * Represents file manager application. Starting point for the application
 * interaction.
 */
class FileManagerApp {
  async run() {
    try {
      window.appID = loadTimeData.getInteger('WINDOW_NUMBER');
    } catch (e) {
      reportGlitch(GlitchType.CAUGHT_EXCEPTION);
      console.warn('Failed to get the app ID', e);
    }

    // Temporarily remove window.cr.webUI* while the foreground script loads.
    const origWebUIResponse = window.webUIResponse;
    const origWebUIListenerCallback = window.webUIListenerCallback;
    delete window.cr.webUIResponse;
    delete window.cr.webUIListenerCallback;

    await new ScriptLoader('foreground/js/main.js', {type: 'module'}).load();

    // Restore the window.cr.webUI* objects.
    window.cr.webUIResponse = origWebUIResponse;
    window.cr.webUIListenerCallback = origWebUIListenerCallback;

    console.warn(
        '%cYou are running Files System Web App',
        'font-size: 2em; background-color: #ff0; color: #000;');
  }
}

const app = new FileManagerApp();
app.run();
