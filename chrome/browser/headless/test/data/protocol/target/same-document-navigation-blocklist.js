// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      'Tests that same-document navigation to a blocklisted URL detaches ' +
      'both tab and page targets.');

  const childTargetManager =
      testRunner.createChildTargetManagerFor(tabTargetSession);
  await childTargetManager.startAutoAttach();
  const {targetId: pageTargetId} =
      (await childTargetManager.findAttachedSessionPrimaryMainFrame()
           .protocol.Target.getTargetInfo())
          .result.targetInfo;

  // Attach an independent browser-level session directly to the page target so
  // its detachment is tested independently of the tab target session.
  const pageSession =
      await testRunner.browserSession().attachChild(pageTargetId);

  const initialUrl = `http://a.test:${
      window.location.port}/protocol/inspector-protocol-page.html`;
  await pageSession.navigate(initialUrl);

  const bp = testRunner.browserP();
  const tabDetachedPromise = bp.Target.onceDetachedFromTarget(
      event => event.params.targetId !== pageTargetId);
  const pageDetachedPromise = bp.Target.onceDetachedFromTarget(
      event => event.params.targetId === pageTargetId);

  // Do not await evaluate, as force-detaching the session drops in-flight
  // CDP responses.
  pageSession.evaluate(() => {
    history.pushState({}, '', '/protocol/blocked_route');
  });

  const tabDetached = (await tabDetachedPromise).params;
  const pageDetached = (await pageDetachedPromise).params;
  testRunner.log(tabDetached, 'Tab target detached: ');
  testRunner.log(pageDetached, 'Page target detached: ');

  testRunner.log(
      (await bp.Target.attachToTarget(
           {targetId: tabDetached.targetId, flatten: true}))
          .error,
      'Re-attaching to tab target: ');

  testRunner.log(
      (await bp.Target.attachToTarget({targetId: pageTargetId, flatten: true}))
          .error,
      'Re-attaching to page target: ');

  testRunner.completeTest();
})
