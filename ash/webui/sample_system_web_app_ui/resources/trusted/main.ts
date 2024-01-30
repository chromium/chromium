// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {callbackRouter, pageHandler} from './page_handler.js';
import {PageCallbackRouter, PageHandlerRemote} from './sample_system_web_app_ui.mojom-webui.js';

const first = document.querySelector<HTMLInputElement>('#number1')!;
const second = document.querySelector<HTMLInputElement>('#number2')!;
const additional = document.querySelector<HTMLInputElement>('#additional')!;

const result = document.querySelector('#result')!;

declare global {
  interface Window {
    pageHandler: PageHandlerRemote;
    callbackRouter: PageCallbackRouter;
    eventCount: Map<string, number>;
  }
}

const workerUrlPolicy = window.trustedTypes!.createPolicy('worker-js-static', {
  createScriptURL: (_ignored: string) =>
      'chrome://sample-system-web-app/worker.js',
});

// Currently TypeScript doesn't support trusted types so cast TrustedScriptURL
// to URL. See https://github.com/microsoft/TypeScript/issues/30024.
const myWorker = new SharedWorker(
    workerUrlPolicy.createScriptURL('') as unknown as URL, {type: 'module'});

first.onchange = () => {
  myWorker.port.postMessage([first.value, second.value]);
};

second.onchange = () => {
  myWorker.port.postMessage([first.value, second.value]);
};

myWorker.port.onmessage = (event: any) => {
  result.textContent = event.data[0];
  additional.value = event.data[1];
};

// Exposes the pageHandler to the user as a window's global variable for
// testing.
window.pageHandler = pageHandler;
window.callbackRouter = callbackRouter;
window.eventCount = new Map();

// Example of adding an event listener for `OnEventOccurred`.
callbackRouter.onEventOccurred.addListener((name: string) => {
  document.querySelector<HTMLInputElement>('#mojo-event')!.value = name;
  window.eventCount.set(name, 1 + (window.eventCount.get(name) || 0));
});

// Example of sending information to the browser process.
pageHandler.send(`message at ${Date.now()}`);

// Example of getting information from the browser process.
(async () => {
  // Mojo results get wrapped in a "response" object that contains
  // a member for each of the Mojo callback's argument, in this case
  // a `preferences` member.
  const {preferences} = await pageHandler.getPreferences();
  document.querySelector<HTMLInputElement>('#background')!.value =
      preferences.background;
  document.querySelector<HTMLInputElement>('#foreground')!.value =
      preferences.foreground;
})();

const mojoButton = document.querySelector<HTMLButtonElement>('#do-something')!;
mojoButton.onclick = () => {
  pageHandler.doSomething();
};
