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
 *      this.pageHandler_ = new PageHandlerRemote();
 *      const factoryRemote = PageHandlerFactory.getRemote();
 *      factoryRemote.createPageHandler(
 *         this.pageHandler_.$.bindNewPipeAndPassReceiver(),
 *       this.callbackRouter.$.bindNewPipeAndPassRemote());
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

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './sample_system_web_app_ui.mojom-webui.js';

// Used to make calls on the remote PageHandler interface. Singleton that client
// modules can use directly.
export const pageHandler = new PageHandlerRemote();

// Use this subscribe to events e.g.
// `callbackRouter.onEventOccurred.addListener(handleEvent)`.
export const callbackRouter = new PageCallbackRouter();

// Use PageHandlerFactory to create a connection to PageHandler.
const factoryRemote = PageHandlerFactory.getRemote();
factoryRemote.createPageHandler(
    pageHandler.$.bindNewPipeAndPassReceiver(),
    callbackRouter.$.bindNewPipeAndPassRemote());

ColorChangeUpdater.forDocument().start();
