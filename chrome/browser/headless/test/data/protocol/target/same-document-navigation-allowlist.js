// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const initialUrl = `http://a.test:${
      window.location.port}/protocol/inspector-protocol-page.html`;

  const {page, session} = await testRunner.startURL(
      initialUrl,
      'Tests that same-document navigation away from an allowlisted URL ' +
          'detaches the session.');

  const bp = testRunner.browserP();

  const detachedPromise = bp.Target.onceDetachedFromTarget(
      event => event.params.targetId === page.targetId());

  // Do not await evaluate, as force-detaching the session drops in-flight
  // CDP responses.
  session.evaluate(() => {
    history.replaceState({}, '', '/protocol/other_route');
  });

  testRunner.log((await detachedPromise).params, 'Page target detached: ');

  testRunner.log(
      (await bp.Target.attachToTarget(
           {targetId: page.targetId(), flatten: true}))
          .error,
      'Re-attaching to page target: ');

  testRunner.completeTest();
})
