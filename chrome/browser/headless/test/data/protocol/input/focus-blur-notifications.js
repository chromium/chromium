// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests focus/blur notifications.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
    if (text === 'quit') {
      testRunner.completeTest();
    }
  });

  await dp.Page.enable();

  // Chrome optimizes away onfocus/onblur notifications if the target page
  // is not active, so activate it explicitly. Note that this is not needed
  // for the old headless which implicitly sets focus to a navigated page.
  dp.Page.bringToFront();

  dp.Page.navigate(
      {url: testRunner.url('/resources/focus-blur-notifications.html')});
})
