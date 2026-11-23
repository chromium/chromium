// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log(
      'Tests that Target.createTarget on a page session creates targets in ' +
      'the browser context of that page.\n');

  const browserSession = await testRunner.attachFullBrowserSession();
  const bp = browserSession.protocol;

  const {result: {defaultBrowserContextId}} =
      await bp.Target.getBrowserContexts();
  const {result: {browserContextId}} =
      await bp.Target.createBrowserContext({disposeOnDetach: true});

  async function describeContext(targetId) {
    const {result: {targetInfo}} = await bp.Target.getTargetInfo({targetId});
    if (targetInfo.browserContextId === browserContextId) {
      return 'page context';
    }
    if (targetInfo.browserContextId === defaultBrowserContextId) {
      return 'default context';
    }
    return 'unexpected context';
  }

  async function logCreateTarget(title, protocol, params) {
    const {result, error} =
        await protocol.Target.createTarget({url: 'about:blank', ...params});
    if (error) {
      testRunner.log(`${title}: error: ${error.message}`);
      return;
    }
    testRunner.log(`${title}: ${await describeContext(result.targetId)}`);
  }

  const {result: {targetId}} =
      await bp.Target.createTarget({browserContextId, url: 'about:blank'});
  const {result: {sessionId}} =
      await bp.Target.attachToTarget({targetId, flatten: true});
  const dp = testRunner.createSessionFor(sessionId).protocol;

  await logCreateTarget('Page session', dp, {});
  await logCreateTarget('Page session, hidden', dp, {hidden: true});
  await logCreateTarget(
      'Page session with browserContextId', dp,
      {browserContextId: defaultBrowserContextId});
  await logCreateTarget(
      'Page session with browserContextId, hidden', dp,
      {browserContextId: defaultBrowserContextId, hidden: true});
  await logCreateTarget('Browser session', bp, {});

  await bp.Target.disposeBrowserContext({browserContextId});
  testRunner.completeTest();
})
