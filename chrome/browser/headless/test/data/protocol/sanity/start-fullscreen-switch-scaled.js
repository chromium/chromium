// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={3000x2000 devicePixelRatio=2.0}
// META: --start-fullscreen
//
(async function(testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests --start-fullscreen switch with pixel scaling.');

  // Wait for the browser window to be resized to fullscreen and ready.
  await session.evaluateAsync(() => new Promise(resolve => {
                                function checkSize() {
                                  if (window.outerWidth === screen.width &&
                                      window.outerHeight === screen.height) {
                                    resolve();
                                    return;
                                  }
                                  window.addEventListener(
                                      'resize', checkSize, {once: true});
                                }
                                checkSize();
                              }));

  const expression = `
      let lines = [];
      lines.push('Screen: ' + screen.width + 'x' + screen.height);
      lines.push('Outer: ' + window.outerWidth + 'x' + window.outerHeight);
      lines.join(', ');
    `;

  const result = await session.evaluate(expression);

  testRunner.log(result);

  testRunner.completeTest();
})
