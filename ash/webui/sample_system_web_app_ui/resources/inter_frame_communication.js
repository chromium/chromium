// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file demonstrates how a chrome:// page can communicate with its
// embedded chrome-untrusted:// child page.

import {ParentTrustedPageReceiver} from '/ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_shared_ui.mojom-webui.js';
import {callbackRouter} from './page_handler.js';

/**
 * Implements ParentTrustedPage interface to handle requests from the child
 * page.
 *
 * Note: If you expect to have multiple listeners for your interface, consider
 * using a CallbackRouter instead. CallbackRouter provides a more event-like
 * API that makes it easier to have multiple listeners.
 *
 * @implements {ash.mojom.sample_swa.ParentTrustedPage}
 */
class ParentTrustedPageImpl {
  /**
   * @param {ash.mojom.sample_swa.ParentTrustedPagePendingReceiver}
   *     pendingReceiver
   */
  constructor(pendingReceiver) {
    this.receiver_ = new ParentTrustedPageReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);
  }

  async doSomethingForChild(task) {
    document.querySelector('#child-task').innerText = task;

    // Mojo interface's JS implementation should return an Object, even if the
    // method only has one return value.
    //
    // Each field should match their return value name defined in .mojom file.
    return {resp: 'Task done'};
  }
}

// A promise that resolves when child page is ready. Other modules wishing to
// use childPage need to wait for the promise.
export const childPageReady = new Promise(resolve => {
  callbackRouter.createParentPage.addListener(
      (childPageRemote, parentPagePendingReceiver) => {
        resolve({
          childPage: childPageRemote,
          parentPageReceiver:
              new ParentTrustedPageImpl(parentPagePendingReceiver)
        });
      });
});

// Expose for testing.
window.childPageReady = childPageReady;

childPageReady.then(({childPage}) => {
  childPage.doSomethingForParent('Hello from chrome://');
});
