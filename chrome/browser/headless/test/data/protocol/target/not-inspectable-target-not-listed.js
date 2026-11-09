// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const allowedUrl = `http://a.test:${
      window.location.port}/protocol/inspector-protocol-page.html`;
  const blockedUrl =
      `http://a.test:${window.location.port}/protocol/blocked_route`;

  await testRunner.startURL(
      allowedUrl,
      'Tests that targets that can\'t be inspected aren\'t listed.');

  const bp = testRunner.browserP();
  const {targetId} =
      (await bp.Target.createTarget({url: 'about:blank'})).result;
  const blockedSession =
      await testRunner.browserSession().attachChild(targetId);

  // The session is detached once the page commits the blocklisted URL.
  const detachedPromise = bp.Target.onceDetachedFromTarget(
      event => event.params.targetId === targetId);
  // Do not await navigate, as force-detaching the session drops in-flight
  // CDP responses.
  blockedSession.protocol.Page.navigate({url: blockedUrl});
  await detachedPromise;

  // Only the page at the allowed URL is listed among the pages on a.test.
  const {targetInfos} = (await bp.Target.getTargets()).result;
  const listedPaths = targetInfos.map(info => info.url)
                          .filter(url => url.startsWith('http://a.test:'))
                          .map(url => new URL(url).pathname);
  testRunner.log('Listed pages on a.test: ' + listedPaths.join(', '));

  testRunner.completeTest();
})
