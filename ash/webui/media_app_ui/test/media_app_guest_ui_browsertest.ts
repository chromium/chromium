// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://media-app. */

/// <reference path="media_app.d.ts" />
/// <reference path="test_api.d.ts" />

import {GUEST_TEST} from './guest_query_receiver.js';
import {ReceivedFileList} from './receiver.js';

export function eventToPromise(eventType: string, target: EventTarget) {
  return new Promise<Event>(function (resolve) {
    target.addEventListener(eventType, function f(e: Event) {
      target.removeEventListener(eventType, f);
      resolve(e);
    });
  });
}

// Test web workers can be spawned from chrome-untrusted://media-app. Errors
// will be logged in console from web_ui_browser_test.cc.
GUEST_TEST('GuestCanSpawnWorkers', async () => {
  const worker = new Worker('test_worker.js');
  const workerResponse = new Promise<MessageEvent>((resolve, reject) => {
    /**
     * Registers onmessage event handler.
     * @param {MessageEvent} event Incoming message event.
     */
    worker.onmessage = resolve;
    worker.onerror = reject;
  });

  const MESSAGE = 'ping/pong message';

  worker.postMessage(MESSAGE);

  assertEquals('ping/pong message', (await workerResponse).data);
});

// Test that language is set correctly on the guest iframe.
GUEST_TEST('GuestHasLang', () => {
  assertEquals(document.documentElement.lang, 'en-US');
});

GUEST_TEST('GuestLoadsLoadTimeData', () => {
  // TODO(b/314827247): Add types for `sandboxed_load_time_data.js`.
  const loadTimeData = (window as any)['loadTimeData'];
  // Check `LoadTimeData` exists on the global window object.
  chai.assert.isTrue(loadTimeData !== undefined);
  // Check data loaded into `LoadTimeData` by "strings.js" via
  // `source->UseStringsJs()` exists.
  assertEquals(loadTimeData.getValue('appLocale'), 'en-US');
});

// Test can load files with CSP restrictions. We expect `error` to be called
// as these tests are loading resources that don't exist. Note: we can't violate
// CSP in tests or Js Errors will cause test failures.
// TODO(crbug.com/40156902): PDF loading tests should also appear here, they are
// currently in media_app_integration_browsertest.cc due to 'wasm-eval' JS
// errors.
GUEST_TEST('GuestCanLoadWithCspRestrictions', async () => {
  // Can load images served from chrome-untrusted://media-app/.
  const image = new Image();
  image.src = 'chrome-untrusted://media-app/does-not-exist.png';
  await eventToPromise('error', image);

  // Can load image data urls.
  const imageData = new Image();
  imageData.src = 'data:image/png;base64,iVBORw0KG';
  await eventToPromise('error', imageData);

  // Can load image blobs.
  const imageBlob = new Image();
  imageBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await eventToPromise('error', imageBlob);

  // Can load video blobs.
  const videoBlob = document.createElement('video');
  videoBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await eventToPromise('error', videoBlob);
});

GUEST_TEST('GuestStartsWithDefaultFileList', async () => {
  chai.assert.isDefined(window.customLaunchData);
  chai.assert.isDefined(window.customLaunchData.files);
  chai.assert.isTrue(window.customLaunchData.files.length === 0);
});

GUEST_TEST('GuestFailsToFetchMissingFonts', async () => {
  let error!: TypeError;
  try {
    await fetch('/fonts/NotAFont.ttf');
  } catch (e: unknown) {
    error = e as TypeError;
  }

  // Note failed webui requests are completely missing response headers, so
  // fetch() will throw rather than returning a response.status of 404.
  assertEquals(error.name, 'TypeError');
  assertEquals(error.message, 'Failed to fetch');
});

GUEST_TEST('GuestCanFilterInPlace', async () => {
  function makeTestFile(name: number) {
    return {
      token: 0,
      file: null,
      name: `${name}`,
      error: '',
      canDelete: true,
      canRename: true,
    };
  }
  const message = {currentFileIndex: 0, files: [0, 1, 2].map(makeTestFile)};
  const fileList = new ReceivedFileList(message);

  fileList.filterInPlace(f => f.name !== '1');

  assertEquals(fileList.length, 2);
  assertEquals(fileList.currentFileIndex, 0);
  assertEquals(fileList.item(0)!.name, '0');
  assertEquals(fileList.item(1)!.name, '2');

  fileList.filterInPlace(() => false);

  assertEquals(fileList.length, 0);
  assertEquals(fileList.currentFileIndex, -1);
});
