// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const html = `<input></input>
    <input id='second'></input>
  `;
  const {page, session, dp} =
      await testRunner.startHTML(html, 'Tests DOM.focus method.');

  testRunner.log(await session.evaluate(getActiveElement));
  const document = (await dp.DOM.getDocument()).result.root;
  const node = (await dp.DOM.querySelector({
                 nodeId: document.nodeId,
                 selector: '#second'
               })).result;
  await dp.DOM.focus({nodeId: node.nodeId});
  testRunner.log(await session.evaluate(getActiveElement));
  testRunner.completeTest();

  function getActiveElement() {
    const element = document.activeElement;
    return element ? (element.id || element.tagName) : '(none)';
  }
})
