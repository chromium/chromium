// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Convenience module to bind to initialize a PageHandler
 * remote i.e. a PageHandler that we can use to talk to the browser.
 *
 * For more complex interfaces, e.g. interfaces where a many arguments need
 * to be converted to Mojo structs, it's recommended to create a wrapper class
 * around PageHandlerRemote. This allows clients to have a centralized place
 * to perform the conversion. For example a wrapper class for this PageHandler
 * would roughly be:
 *
 *  class PageHandlerWrapper {
 *    constructor() {
 *      const {handler, callbackRouter} = browserProxyFactory.getInstance();
 *      this.pageHandler_ = handler;
 *      this.callbackRouter = callbackRouter;
 *    }
 *
 *    send(message) {
 *      this.pageHandler_.send(message);
 *    }
 *
 *    doSomething() {
 *      this.pageHandler_.doSomething();
 *    }
 *
 *    async getPreferences() {
 *      return this.pageHandler_.getPreferences();
 *    }
 *  }
 */

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {browserProxyFactory} from './sample_system_web_app_ui.mojom-webui.js';

export const {handler: pageHandler, callbackRouter} =
    browserProxyFactory.getInstance();

ColorChangeUpdater.forDocument().start();
