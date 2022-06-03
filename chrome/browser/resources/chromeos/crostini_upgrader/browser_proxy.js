// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(lxj): use es6 module when it is ready crbug/1004256
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './crostini_upgrader.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class BrowserProxy {
  constructor() {
    /** @type {chromeos.crostiniUpgrader.mojom.PageCallbackRouter} */
    this.callbackRouter =
        new chromeos.crostiniUpgrader.mojom.PageCallbackRouter();
    /** @type {chromeos.crostiniUpgrader.mojom.PageHandlerRemote} */
    this.handler = new chromeos.crostiniUpgrader.mojom.PageHandlerRemote();

    const factory =
        chromeos.crostiniUpgrader.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}

addSingletonGetter(BrowserProxy);
