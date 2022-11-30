// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(() => {

let cls_expectations = [];

function add_debug_names(expectation) {
  if (!expectation.sources)
    return;
  for (let source of expectation.sources) {
    let node = source.node;
    if (!node)
      continue;
    source.debugName = node.tagName;
    if (node.id)
      source.debugName += ` id='${node.id}'`;
    if (node.className)
      source.debugName += ` class='${node.className}'`;
  }
}

window.cls_expect = (watcher, expectation) => {
  watcher.checkExpectation(expectation);
  add_debug_names(expectation);
  cls_expectations.push(expectation);
};

window.cls_run_tests = new Promise((resolve, reject) => {
  add_completion_callback((tests, harness_status) => {
    if (harness_status.status != 0) {
      reject(harness_status.message);
      return;
    }
    for (let test of tests) {
      if (test.status != 0 /* PASS */) {
        reject(test.message);
        return;
      }
    }
    resolve(cls_expectations);
  });
});

})();
