// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const {boolean}
 */
window.isSWA = true;

import './crt0.js';

/**
 * Load modules.
 */
import {BrowserProxy} from './browser_proxy.js';
import {ScriptLoader} from './script_loader.js';
import {promisify} from 'chrome://file-manager/common/js/api.js';
import {GlitchType, reportGlitch} from 'chrome://file-manager/common/js/glitch.js';
import {VolumeManagerImpl} from 'chrome://file-manager/background/js/volume_manager_impl.js';
import 'chrome://file-manager/background/js/metrics_start.js';
import {background} from 'chrome://file-manager/background/js/background.js';
import './test_util_swa.js';

/**
 * Represents file manager application. Starting point for the application
 * interaction.
 */
class FileManagerApp {
  constructor() {
    /**
     * Creates a Mojo pipe to the C++ SWA container.
     * @private @const {!BrowserProxy}
     */
    this.browserProxy_ = new BrowserProxy();
  }

  /** @return {!BrowserProxy} */
  get browserProxy() {
    return this.browserProxy_;
  }

  /**
   * Start-up: load the page scripts in order: fakes first (to provide chrome.*
   * API that the files app foreground scripts expect for initial render), then
   * the files app foreground scripts.
   */
  async run() {
    try {
      const win = await promisify(chrome.windows.getCurrent);
      window.appID = win.id;
    } catch (e) {
      reportGlitch(GlitchType.CAUGHT_EXCEPTION);
      console.warn('Failed to get the app ID', e);
    }

    await new ScriptLoader('file_manager_fakes.js', {type: 'module'}).load();

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
