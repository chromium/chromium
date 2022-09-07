// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://sample-system-web-app.
 */

GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://sample-system-web-app';
const UNTRUSTED_HOST_ORIGIN = 'chrome-untrusted://sample-system-web-app';

// TODO:(crbug.com/1262025): We should avoid using `var`.
//
// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var SampleSystemWebAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get isAsync() {
    return true;
  }
};

// Tests that chrome://sample-system-web-app runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('SampleSystemWebAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  const header = document.querySelector('header');

  assertEquals(header.innerText, 'Sample System Web App');
  assertEquals(document.location.origin, HOST_ORIGIN);
  testDone();
});

// Test the ability to get information from the page handler.
TEST_F('SampleSystemWebAppUIBrowserTest', 'FetchPreferences', async () => {
  const {preferences} = await window.pageHandler.getPreferences();
  assertDeepEquals({background: '#ffffff', foreground: '#000000'}, preferences);
  testDone();
});

// Test the ability to trigger work in the page handler.
TEST_F('SampleSystemWebAppUIBrowserTest', 'DoSomething', async () => {
  const pageHandler = window.pageHandler;
  const callbackRouter = window.callbackRouter;

  // Now execute our test: zero the event count and call doSomething.
  window.eventCount.set('DoSomething is done', 0);
  pageHandler.doSomething();

  // Ensure the DoSomething() is called on the browser side.
  await pageHandler.$.flushForTesting();

  // Await the C++ process to call back with the event.
  await callbackRouter.$.flush();
  // Verify the expected event count.
  assertEquals(1, window.eventCount.get('DoSomething is done'));

  testDone();
});

// eslint-disable-next-line no-var
var SampleSystemWebAppUIUntrustedBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN + '/inter_frame_communication.html';
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get isAsync() {
    return true;
  }
};

// Tests that chrome://sample-system-web-app/inter_frame_communication.html
// embeds a chrome-untrusted:// iframe.
TEST_F(
    'SampleSystemWebAppUIUntrustedBrowserTest', 'HasChromeUntrustedIframe',
    () => {
      const iframe = document.querySelector('iframe');
      window.onmessage = (event) => {
        if (event.data.id === 'post-message') {
          assertEquals(event.origin, UNTRUSTED_HOST_ORIGIN);
          assertEquals(event.data.success, true);
          testDone();
        }
      };
      iframe.contentWindow.postMessage('hello', UNTRUSTED_HOST_ORIGIN);
    });

// Tests that chrome://sample-system-web-app/inter_frame_communication.html
// can communicate with its embedded chrome-untrusted:// iframe via Mojo
// method calls.
TEST_F('SampleSystemWebAppUIUntrustedBrowserTest', 'MojoMethodCall', () => {
  window.onmessage = (event) => {
    if (event.data.id === 'mojo-method-call-resp') {
      assertEquals(event.data.resp, 'Task done');
      testDone();
    }
  };

  const iframe = document.querySelector('iframe');
  iframe.contentWindow.postMessage(
      {id: 'test-mojo-method-call'}, UNTRUSTED_HOST_ORIGIN);
});

TEST_F('SampleSystemWebAppUIUntrustedBrowserTest', 'MojoMessage', () => {
  window.onmessage = (event) => {
    if (event.data.id === 'mojo-did-receive-task') {
      assertEquals(event.data.task, 'Hello from chrome://');
      testDone();
    }
  };

  window.childPageReady.then(({childPage}) => {
    childPage.doSomethingForParent('Hello from chrome://');
  });
});
