// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {dp} =
      await testRunner.startBlank(`Tests fullscreen browser window size.`);

  await dp.Page.enable();

  async function getWindowSize(windowId) {
    const {result: {bounds}} = await dp.Browser.getWindowBounds({windowId});
    return {width: bounds.width, height: bounds.height};
  }

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  const intialSize = await getWindowSize(windowId);

  await dp.Browser.setWindowBounds(
      {windowId, bounds: {windowState: 'fullscreen'}});
  await dp.Page.onceFrameResized();

  const fullscreenSize = await getWindowSize(windowId);
  if (fullscreenSize.width > intialSize.width &&
      fullscreenSize.height > intialSize.height) {
    testRunner.log(`Success`);
  } else {
    testRunner.log(`Failure:`);
    testRunner.log(intialSize);
    testRunner.log(fullscreenSize);
  }

  testRunner.completeTest();
})
