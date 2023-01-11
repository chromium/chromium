// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://file-manager/background/js/metrics_start.js';
import './test_util_swa.js';

import {background} from 'chrome://file-manager/background/js/file_manager_base.js';
import {VolumeManagerImpl} from 'chrome://file-manager/background/js/volume_manager_impl.js';
import {GlitchType, reportGlitch} from 'chrome://file-manager/common/js/glitch.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

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

    await new ScriptLoader('foreground/js/main.js', {type: 'module'}).load();

    console.warn(
        '%cYou are running Files System Web App',
        'font-size: 2em; background-color: #ff0; color: #000;');
  }
}

const app = new FileManagerApp();
app.run();
