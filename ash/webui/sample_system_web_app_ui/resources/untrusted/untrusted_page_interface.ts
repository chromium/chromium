// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChildUntrustedPageReceiver, ParentTrustedPageRemote} from './sample_system_web_app_shared_ui.mojom-webui.js';
import {UntrustedPageInterfacesFactory} from './sample_system_web_app_untrusted_ui.mojom-webui.js';

export const PARENT_PAGE_ORIGIN = 'chrome://sample-system-web-app';

/**
 * Implements ChildUntrustedPage interface to handle requests from the parent
 * page.
 *
 * Note: If you expect to have multiple listeners for your interface, consider
 * using a CallbackRouter instead. CallbackRouter provides a more event-like
 * API that makes it easier to have multiple listeners.
 *
 * @implements {ash.mojom.sample_swa.ChildUntrustedPage}
 */
class ChildUntrustedPageImpl {
  private receiver_ = new ChildUntrustedPageReceiver(this);

  // Returns a Mojo remote that should be send to the parent page to be bound.
  bindNewPipeAndPassRemote() {
    return this.receiver_.$.bindNewPipeAndPassRemote();
  }

  doSomethingForParent(task: string) {
    document.querySelector<HTMLParagraphElement>('#parent-task')!.innerText =
        task;

    // For testing, report the received task.
    window.parent.postMessage(
        {id: 'mojo-did-receive-task', task}, PARENT_PAGE_ORIGIN);
  }
}

// Communication between this page and the parent page.
export const parentPage = new ParentTrustedPageRemote();
export const childPageImpl = new ChildUntrustedPageImpl();

if (window.parent) {
  const factoryRemote = UntrustedPageInterfacesFactory.getRemote();
  factoryRemote.createParentPage(
      childPageImpl.bindNewPipeAndPassRemote(),
      parentPage.$.bindNewPipeAndPassReceiver(),
  );
} else {
  // Opened as a top-level page. Show a warning to user perhaps.
}
