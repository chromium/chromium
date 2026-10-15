// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const mainUrl = `http://a.test:${
      window.location.port}/protocol/inspector-protocol-page.html`;
  const oopifUrl = `http://b.test:${
      window.location.port}/protocol/inspector-protocol-page.html`;

  const {session, dp} = await testRunner.startURL(
      mainUrl,
      'Tests that same-document navigation to a blocklisted URL in an OOPIF ' +
          'detaches only the OOPIF target.');

  const childTargetManager = testRunner.createChildTargetManagerFor(session);
  await childTargetManager.startAutoAttach();

  await session.evaluateAsync(url => {
    return new Promise(resolve => {
      const frame = document.createElement('iframe');
      frame.src = url;
      frame.addEventListener('load', resolve);
      document.body.appendChild(frame);
    });
  }, oopifUrl);

  const oopifSession = childTargetManager.findAttachedSession(
      targetInfo => targetInfo.type === 'iframe');

  const oopifDetachedPromise = dp.Target.onceDetachedFromTarget();

  // Do not await evaluate, as force-detaching the session drops in-flight
  // CDP responses.
  oopifSession.evaluate(() => {
    history.pushState({}, '', '/protocol/blocked_route');
  });

  const oopifDetached = (await oopifDetachedPromise).params;
  testRunner.log(oopifDetached, 'OOPIF target detached: ');

  testRunner.log(
      'Main page still inspectable: ' +
      await session.evaluate(() => location.pathname));

  testRunner.log(
      (await testRunner.browserP().Target.attachToTarget(
           {targetId: oopifDetached.targetId, flatten: true}))
          .error,
      'Re-attaching to OOPIF target: ');

  testRunner.completeTest();
})
