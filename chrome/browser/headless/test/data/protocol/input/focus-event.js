// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session} = await testRunner.startBlank('Tests focus event.');

  testRunner.log(await session.evaluateAsync(tryFocusEvent));

  testRunner.completeTest();

  async function tryFocusEvent() {
    const input = document.createElement('input');
    document.body.appendChild(input);

    let eventFired = false;
    input.addEventListener('focus', () => {
      eventFired = true;
    });

    input.focus();

    await new Promise(requestAnimationFrame);

    return eventFired ? 'Focus event fired' : 'Focus event NOT fired';
  }
})
