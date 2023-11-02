// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const workerUrlPolicy = trustedTypes.createPolicy(
    'worker-js-static',
    {createScriptURL: () => 'chrome://sample-system-web-app/worker.js'});
const myWorker = new SharedWorker(workerUrlPolicy.createScriptURL(''));

myWorker.port.onmessage = (e) => {
  window.close();
  myWorker.port.close();
};

myWorker.port.postMessage(['doubler', Math.floor(Math.random() * 3)]);
