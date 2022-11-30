// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function triggerDocsCanvasAnnotationMode() {
  const scriptContents = `
    window['_docs_annotate_canvas_by_ext'] = "${chrome.runtime.id}";
  `;
  const script = document.createElement('script');
  script.innerHTML = scriptContents;
  document.documentElement.appendChild(script);
}

// Docs renders content in Canvas without annotations by default. This script is
// used to trigger annotated Canvas which allows the a11y tree to be built for
// the document contents. This needs to run within the page's context, so
// install a script to be executed.
triggerDocsCanvasAnnotationMode();
