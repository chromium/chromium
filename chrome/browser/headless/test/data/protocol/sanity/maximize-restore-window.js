// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(`Tests browser window maximize and restore.`);

  await dp.Page.enable();

  async function logWindowState(text, windowId) {
    const {result: {bounds}} = await dp.Browser.getWindowBounds({windowId});
    const visibilityState = await session.evaluate(`document.visibilityState`);
    testRunner.log(`${text}: ${bounds.windowState} ${visibilityState}`);
  }

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  await logWindowState('Initial', windowId);

  await dp.Browser.setWindowBounds(
      {windowId, bounds: {windowState: 'maximized'}});
  await dp.Page.onceFrameResized();
  await logWindowState('Maximized', windowId);

  await dp.Browser.setWindowBounds({windowId, bounds: {windowState: 'normal'}});
  await dp.Page.onceFrameResized();
  await logWindowState('Restored', windowId);

  testRunner.completeTest();
})
