// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const html = `<!doctype html>
    <html><body>
    <input type="text" id="input" value="input_value" autofocus>
    </body></html>
  `;

  const {page, session, dp} = await testRunner.startHTML(
      html, `Tests input field clipboard operations.`);

  async function logElementValue(id) {
    const value = await session.evaluate(`
      document.getElementById("${id}").value;
    `);
    testRunner.log(`${id}: ${value}`);
  }

  async function sendKey(
      text, modifiers = 0, commands = []) {
    const keyCode = text.charCodeAt(0);
    await dp.Input.dispatchKeyEvent({
      type: 'keyDown',
      modifiers: modifiers,
      text: text,
      nativeVirtualKeyCode: keyCode,
      commands: commands
    });
    await dp.Input.dispatchKeyEvent({
      type: 'keyUp',
      modifiers: modifiers,
      nativeVirtualKeyCode: keyCode
    });
  }

  await dp.Browser.grantPermissions({permissions: ['clipboardReadWrite']});

  const kControl = 2;
  const kCommand = 4;
  const mod = navigator.platform.includes('Mac') ? kCommand : kControl;

  await logElementValue('input');
  await sendKey('a', mod, ['selectAll']);
  await sendKey('c', mod, ['copy']);

  await sendKey('1');
  await sendKey('2');
  await sendKey('3');
  await logElementValue('input');

  // Don't send Ctrl+A here because this would cause clipboard copy on
  // systems that support selection clipboard, e.g. Linux.
  await sendKey('v', mod, ['paste']);
  await logElementValue('input');

  testRunner.completeTest();
})
