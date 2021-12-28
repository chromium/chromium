// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {pageHandler, callbackRouter} from './page_handler.js';

const first = document.querySelector('#number1');
const second = document.querySelector('#number2');
const additional = document.querySelector('#additional');

const result = document.querySelector('#result');
const form = document.querySelector('form');

const workerUrlPolicy = trustedTypes.createPolicy(
    'worker-js-static',
    {createScriptURL: () => 'chrome://sample-system-web-app/worker.js'});
const myWorker = new SharedWorker(workerUrlPolicy.createScriptURL(''));

first.onchange = () => {
  myWorker.port.postMessage([first.value, second.value]);
};

second.onchange = () => {
  myWorker.port.postMessage([first.value, second.value]);
};

myWorker.port.onmessage = (event) => {
  result.textContent = event.data[0];
  additional.value = event.data[1];
};

// Exposes the pageHandler to the user as a window's global variable for
// testing.
window.pageHandler = pageHandler;
window.callbackRouter = callbackRouter;
window.eventCount = new Map();

// Example of adding an event listener for `OnEventOccurred`.
callbackRouter.onEventOccurred.addListener((name) => {
  document.querySelector('#mojo-event').value = name;
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
  document.querySelector('#background').value = preferences.background;
  document.querySelector('#foreground').value = preferences.foreground;
})();

const mojoButton = document.querySelector('#do-something');
mojoButton.onclick = () => {
  pageHandler.doSomething();
};
