// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {session} =
      await testRunner.startBlank(`Tests Ozone screen size override.`);

  const screenSize =
      await session.evaluate('`${screen.width}x${screen.height}`');
  testRunner.log('Screen size: ' + screenSize);

  testRunner.completeTest();
})
