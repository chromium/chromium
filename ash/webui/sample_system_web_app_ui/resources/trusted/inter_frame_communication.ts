// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file demonstrates how a chrome:// page can communicate with its
// embedded chrome-untrusted:// child page.

import {callbackRouter} from './page_handler.js';
import {ChildUntrustedPageRemote, ParentTrustedPage, ParentTrustedPagePendingReceiver, ParentTrustedPageReceiver} from './sample_system_web_app_shared_ui.mojom-webui.js';

/**
 * Implements ParentTrustedPage interface to handle requests from the child
 * page.
 *
 * Note: If you expect to have multiple listeners for your interface, consider
 * using a CallbackRouter instead. CallbackRouter provides a more event-like
 * API that makes it easier to have multiple listeners.
 */
class ParentTrustedPageImpl implements ParentTrustedPage {
  private receiver_ = new ParentTrustedPageReceiver(this);
  constructor(pendingReceiver: ParentTrustedPagePendingReceiver) {
    this.receiver_.$.bindHandle(pendingReceiver.handle);
  }

  async doSomethingForChild(task: string) {
    document.querySelector<HTMLDivElement>('#child-task')!.innerText = task;

    // Mojo interface's JS implementation should return an Object, even if the
    // method only has one return value.
    //
    // Each field should match their return value name defined in .mojom file.
    return {resp: 'Task done'};
  }
}

// A promise that resolves when child page is ready. Other modules wishing to
// use childPage need to wait for the promise.
interface ChildPageReadyResult {
  childPage: ChildUntrustedPageRemote;
  parentPageReceiver: ParentTrustedPage;
}
export const childPageReady = new Promise<ChildPageReadyResult>(resolve => {
  callbackRouter.createParentPage.addListener(
      (childPageRemote: ChildUntrustedPageRemote,
       parentPagePendingReceiver: ParentTrustedPagePendingReceiver) => {
        resolve({
          childPage: childPageRemote,
          parentPageReceiver:
              new ParentTrustedPageImpl(parentPagePendingReceiver),
        });
      });
});

// Expose for testing.
declare global {
  interface Window {
    childPageReady: Promise<{childPage: ChildUntrustedPageRemote}>;
  }
}

window.childPageReady = childPageReady;

childPageReady.then((result: ChildPageReadyResult) => {
  result.childPage.doSomethingForParent('Hello from chrome://');
});
