// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests Target.createTarget() window state handling.`);

  async function tryCreateTarget(windowState) {
    const {targetId} =
        (await session.protocol.Target.createTarget(
             {'url': 'about:blank', windowState, 'newWindow': true}))
            .result;

    const {bounds} = (await dp.Browser.getWindowForTarget({targetId})).result;
    testRunner.log(`Expected: ${windowState}, actual: ${bounds.windowState}`);
  }

  await tryCreateTarget('normal');
  await tryCreateTarget('minimized');
  await tryCreateTarget('maximized');
  await tryCreateTarget('fullscreen');

  testRunner.completeTest();
})
