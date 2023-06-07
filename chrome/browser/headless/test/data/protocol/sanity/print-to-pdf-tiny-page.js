// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const html = `<!doctype html>
    <html><body>Hello, headless world!</body></html>
  `;
  const {dp} = await testRunner.startHTML(
      html, `Tests that printing PDF to a tiny page returns error.`);

  // Print to a PDF with the page size smaller than default margins expecting
  // an error, not a renderer crash, see https://crbug.com/1430696.
  const result = await dp.Page.printToPDF({paperWidth: 0.1, paperHeight: 0.1});
  testRunner.log(result);

  testRunner.completeTest();
})
