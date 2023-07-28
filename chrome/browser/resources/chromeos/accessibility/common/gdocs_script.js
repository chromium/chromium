// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function triggerDocsCanvasAnnotationMode() {
  if (!chrome.runtime || !chrome.runtime.id) {
    // Manifest v3: Not running in extension's runtime.
    // Parse this script's URL to determine extension ID.
    // The URL will look like
    // `chrome-extension://extensionId/common/gdocs_script.js.
    const extensionId = document.currentScript.src.split('/')[2];
    const scriptContents = `
      window['_docs_annotate_canvas_by_ext'] = "${extensionId}";
    `;
    const policy = trustedTypes.createPolicy('gdocsPolicy', {
      createScript: (text) => text,
    });
    const sanitized = policy.createScript(scriptContents);
    eval(sanitized);
  } else {
    // Manifest v2.
    const extensionId = chrome.runtime.id;
    const scriptContents = `
      window['_docs_annotate_canvas_by_ext'] = "${extensionId}";
    `;
    const script = document.createElement('script');
    script.innerHTML = scriptContents;
    document.documentElement.appendChild(script);
  }
}

// Docs renders content in Canvas without annotations by default. This script is
// used to trigger annotated Canvas which allows the a11y tree to be built for
// the document contents. This needs to run within the page's context, so
// install a script to be executed.
triggerDocsCanvasAnnotationMode();
