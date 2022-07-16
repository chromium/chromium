// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const myWorker = new SharedWorker('worker.js');

myWorker.port.onmessage = (e) => {
  window.close();
  myWorker.port.close();
};

myWorker.port.postMessage(['doubler', Math.floor(Math.random() * 3)]);
