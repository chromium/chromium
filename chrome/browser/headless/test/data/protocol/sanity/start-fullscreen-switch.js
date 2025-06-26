// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={1600x1200}
// META: --start-fullscreen
//
(async function(testRunner) {
  const {session} =
      await testRunner.startBlank('Tests --start-fullscreen switch.');

  const expression = `
      let lines = [];
      lines.push('Screen: ' + screen.width + 'x' + screen.height);
      lines.push('Outer: ' + outerWidth + 'x' + outerHeight);
      lines.join(', ');
    `;

  const result = await session.evaluate(expression);

  testRunner.log(result);

  testRunner.completeTest();
})
