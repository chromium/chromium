// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log(
      'Tests that Storage cookie commands on a page target only affect ' +
      'the browser context of that page.\n');

  const browserSession = await testRunner.attachFullBrowserSession();
  const bp = browserSession.protocol;

  const url = 'https://example.test/';

  async function logCookies(title, protocol, params = {}) {
    const {result, error} = await protocol.Storage.getCookies(params);
    if (error) {
      testRunner.log(`${title}: error: ${error.message}`);
      return;
    }
    const cookies = result.cookies.filter(
        c => c.name === 'default_cookie' || c.name === 'page_cookie');
    const names = cookies.map(c => `${c.name}=${c.value}`).sort();
    testRunner.log(`${title}: [${names.join(', ')}]`);
  }

  // Put a cookie into the default browser context through the browser target.
  await bp.Storage.setCookies(
      {cookies: [{name: 'default_cookie', value: 'default_val', url}]});

  const {result: {browserContextId}} =
      await bp.Target.createBrowserContext({disposeOnDetach: true});
  const {result: {targetId}} =
      await bp.Target.createTarget({browserContextId, url: 'about:blank'});
  const {result: {sessionId}} =
      await bp.Target.attachToTarget({targetId, flatten: true});
  const dp = testRunner.createSessionFor(sessionId).protocol;

  async function logAll(title) {
    testRunner.log(title);
    await logCookies('  Page target', dp);
    await logCookies('  Page context', bp, {browserContextId});
    await logCookies('  Default context', bp);
  }

  await logAll('Initial state');

  await dp.Storage.setCookies(
      {cookies: [{name: 'page_cookie', value: 'page_val', url}]});
  await logAll('After Storage.setCookies on page target');

  await dp.Storage.clearCookies();
  await logAll('After Storage.clearCookies on page target');

  const {error} = await dp.Storage.getCookies({browserContextId});
  testRunner.log(
      `Storage.getCookies with browserContextId on page target: ${
          error ? error.message : 'no error'}`);

  await bp.Target.disposeBrowserContext({browserContextId});
  testRunner.completeTest();
})
