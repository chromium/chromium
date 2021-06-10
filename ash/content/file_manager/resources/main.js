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
import {BrowserProxy} from './browser_proxy.js'
import {ScriptLoader} from './script_loader.js'
import {VolumeManagerImpl} from 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/background/js/volume_manager_impl.m.js';
import 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/background/js/metrics_start.m.js';
import {background} from 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/background/js/background.m.js';
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
   * the files app foreground scripts. Note main_scripts.js should have 'defer'
   * true per crbug.com/496525.
   */
  async run() {
    await new ScriptLoader('file_manager_fakes.js', {type: 'module'}).load();

    // Temporarily remove window.cr.webUI* while the foreground script loads.
    const origWebUIResponse = window.webUIResponse;
    const origWebUIListenerCallback = window.webUIListenerCallback;
    delete window.cr.webUIResponse;
    delete window.cr.webUIListenerCallback;

    // Avoid double loading the LoadTimeData strings.
    window.loadTimeData.data_ = null;

    await new ScriptLoader('foreground/js/main.m.js', {type: 'module'}).load();

    // Restore the window.cr.webUI* objects.
    window.cr.webUIResponse = origWebUIResponse;
    window.cr.webUIListenerCallback = origWebUIListenerCallback;

    console.debug('Files app UI loaded');
  }
}

const app = new FileManagerApp();
app.run();
