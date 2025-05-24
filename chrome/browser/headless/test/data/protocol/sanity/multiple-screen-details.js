// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank(`Tests multiple screen configuration.`);

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, dp)).init();

  httpInterceptor.setDisableRequestedUrlsLogging(true);
  httpInterceptor.addResponse(
      'https://example.com/index.html',
      '<html><head><link rel="icon" href="data:,"></head></html>');

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  await session.navigate('https://example.com/index.html');

  const result = await session.evaluateAsync(async () => {
    const screenDetails = await getScreenDetails();
    const screenDetailed = screenDetails.screens.map(s => {
      const lines = [
        `Screen`,
        ` label='${s.label}'`,
        ` ${s.left},${s.top} ${s.width}x${s.height}`,
        ` avail=${s.availLeft},${s.availTop} ${s.availWidth}x${s.availHeight}`,
        ` isPrimary=${s.isPrimary}`,
        ` isExtended=${s.isExtended}`,
        ` isInternal=${s.isInternal}`,
        ` colorDepth=${s.colorDepth}`,
        ` devicePixelRatio=${s.devicePixelRatio}`,
        ` orientation.type=${s.orientation.type}`,
        ` orientation.angle=${s.orientation.angle}`,
      ];
      return lines.join('\n');
    });
    return screenDetailed.join('\n');
  });

  testRunner.log(result);

  testRunner.completeTest();
})
