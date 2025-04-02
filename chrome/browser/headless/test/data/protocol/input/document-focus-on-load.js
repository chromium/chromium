// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const html = `<!doctype html>
    <html>
      <head>
        <script>
          function checkFocus() {
            const log = document.getElementById("log");
            if (document.hasFocus()) {
              log.textContent = "Document has focus on load";
            } else {
              log.textContent = "Document has no focus on load";
            }
          }
        </script>
      </head>

      <body onload="checkFocus()">
        <div id="log">Document focus placeholder</div>
      </body>
    </html>
  `;

  const {session} =
      await testRunner.startHTML(html, `Tests document focus on load.`);

  async function getElementTextContent(id) {
    return session.evaluate(`
      document.getElementById("${id}").textContent;
    `);
  }

  testRunner.log(await getElementTextContent('log'));

  testRunner.completeTest();
})
