// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './crostini_types.mojom-lite.js';
import './crostini_installer.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

export class BrowserProxy {
  constructor() {
    /** @type {ash.crostiniInstaller.mojom.PageCallbackRouter} */
    this.callbackRouter = new ash.crostiniInstaller.mojom.PageCallbackRouter();
    /** @type {ash.crostiniInstaller.mojom.PageHandlerRemote} */
    this.handler = new ash.crostiniInstaller.mojom.PageHandlerRemote();

    const factory = ash.crostiniInstaller.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}

addSingletonGetter(BrowserProxy);
