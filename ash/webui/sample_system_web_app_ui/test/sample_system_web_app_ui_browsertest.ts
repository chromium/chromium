// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://sample-system-web-app.
 */

import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

const HOST_ORIGIN = 'chrome://sample-system-web-app';
const UNTRUSTED_HOST_ORIGIN = 'chrome-untrusted://sample-system-web-app';

suite('SampleSystemWebAppUIBrowserTest', () => {
  // Tests that chrome://sample-system-web-app runs js file and that it goes
  // somewhere instead of 404ing or crashing.
  test('HasChromeSchemeURL', async () => {
    const header = document.querySelector('header');
    assertEquals(header!.innerText, 'Sample System Web App');
    assertEquals(document.location.origin, HOST_ORIGIN);
  });

  // Test the ability to get information from the page handler.
  test('FetchPreferences', async () => {
    const {preferences} = await (window as any).pageHandler.getPreferences();
    assertDeepEquals(
        {background: '#ffffff', foreground: '#000000'}, preferences);
  });

  // Test the ability to trigger work in the page handler.
  test('DoSomething', async () => {
    const pageHandler = (window as any).pageHandler;
    const callbackRouter = (window as any).callbackRouter;

    // Now execute our test: zero the event count and call doSomething.
    (window as any).eventCount.set('DoSomething is done', 0);
    pageHandler.doSomething();

    // Ensure the DoSomething() is called on the browser side.
    await pageHandler.$.flushForTesting();

    // Await the C++ process to call back with the event.
    await callbackRouter.$.flush();
    // Verify the expected event count.
    assertEquals(1, (window as any).eventCount.get('DoSomething is done'));
  });
});

suite('SampleSystemWebAppUIUntrustedBrowserTest', () => {
  // Tests that chrome://sample-system-web-app/inter_frame_communication.html
  // embeds a chrome-untrusted:// iframe.
  test('HasChromeUntrustedIframe', async () => {
    const iframe = document.querySelector('iframe')!;
    const promise = new Promise<void>((resolve) => {
      window.addEventListener('message', (event) => {
        if (event.data.id === 'post-message') {
          assertEquals(event.origin, UNTRUSTED_HOST_ORIGIN);
          assertEquals(event.data.success, true);
          resolve();
        }
      });
    });
    iframe.contentWindow!.postMessage('hello', UNTRUSTED_HOST_ORIGIN);
    await promise;
  });

  // Tests that chrome://sample-system-web-app/inter_frame_communication.html
  // can communicate with its embedded chrome-untrusted:// iframe via Mojo
  // method calls.
  test('MojoMethodCall', async () => {
    const promise = new Promise<void>((resolve) => {
      window.addEventListener('message', (event) => {
        if (event.data.id === 'mojo-method-call-resp') {
          assertEquals(event.data.resp, 'Task done');
          resolve();
        }
      });
    });

    const iframe = document.querySelector('iframe')!;
    iframe.contentWindow!.postMessage(
        {id: 'test-mojo-method-call'}, UNTRUSTED_HOST_ORIGIN);
    await promise;
  });

  test('MojoMessage', async () => {
    const promise = new Promise<void>((resolve) => {
      window.addEventListener('message', (event) => {
        if (event.data.id === 'mojo-did-receive-task') {
          assertEquals(event.data.task, 'Hello from chrome://');
          resolve();
        }
      });
    });

    const {childPage} = await (window as any).childPageReady;
    childPage.doSomethingForParent('Hello from chrome://');
    await promise;
  });
});
