// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const workerUrlPolicy: TrustedTypePolicy =
    window.trustedTypes!.createPolicy('worker-js-static', {
      createHTML: () => '',
      createScriptURL: (_ignored: string) => {
        return 'chrome://sample-system-web-app/worker.js';
      },
      createScript: () => '',
    });

// Currently TypeScript doesn't support trusted types so cast TrustedScriptURL
// to URL. See https://github.com/microsoft/TypeScript/issues/30024.
const myWorker = new SharedWorker(
    workerUrlPolicy.createScriptURL('') as unknown as URL, {type: 'module'});

myWorker.port.onmessage = () => {
  window.close();
  myWorker.port.close();
};

myWorker.port.postMessage(['doubler', Math.floor(Math.random() * 3)]);
