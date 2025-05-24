// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests Target.createTarget() window positioning.`);

  const {targetId} = (await session.protocol.Target.createTarget({
                       'url': 'about:blank',
                       'left': 100,
                       'top': 200,
                       'width': 600,
                       'height': 400,
                       'newWindow': true
                     })).result;

  const {bounds} = (await dp.Browser.getWindowForTarget({targetId})).result;
  testRunner.log(bounds, 'Window bounds: ');

  testRunner.completeTest();
})
