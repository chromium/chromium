// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerRemote} from '/ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom-webui.js';

import {callbackRouter, pageHandler} from './page_handler.js';

declare global {
  interface Window {
    pageHandler: PageHandlerRemote;
    callbackRouter: PageCallbackRouter;
  }
}

window.pageHandler = pageHandler;
window.callbackRouter = callbackRouter;

(async () => {
  const content = document.querySelector<HTMLElement>('#app-top-bar')!;
  content.textContent = 'Welcome to the Face ML app!';
  const {userInfo} = await pageHandler.getCurrentUserInformation();
  document.querySelector<HTMLInputElement>('#user-name')!.value =
      userInfo.userName;
  document.querySelector<HTMLInputElement>('#is-signed-in')!.value =
      userInfo.isSignedIn ? 'Signed In' : 'Not signed in';
  const topbar = document.querySelector<HTMLElement>('#app-top-bar')!;
  topbar.style.display = 'none';
  //(TODO:b/243653034): Populate topbar via page handler.
})();
