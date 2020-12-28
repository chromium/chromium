// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test support for tests driven by C++.
 */

(async function() {
  let module = await import('./nodes/back_button_node.js');
  window.BackButtonNode = module.BackButtonNode;

  module = await import('./focus_ring_manager.js');
  window.FocusRingManager = module.FocusRingManager;

  const focusRingState = {
    'primary': {'role': '', 'name': ''},
    'preview': {'role': '', 'name': ''}
  };
  let expectedType = '';
  let expectedRole = '';
  let expectedName = '';
  let successCallback = null;
  const transcript = [];

  function checkFocusRingState() {
    if (expectedType !== '' &&
        focusRingState[expectedType].role === expectedRole &&
        focusRingState[expectedType].name === expectedName) {
      if (successCallback) {
        transcript.push(
            `Success type=${expectedType} ` +
            `role=${expectedRole} name=${expectedName}`);
        successCallback();
        successCallback = null;
      }
    }
  }

  window.waitForFocusRing = function(type, role, name, callback) {
    transcript.push(`Waiting for type=${type} role=${role} name=${name}`);
    expectedType = type;
    expectedRole = role;
    expectedName = name;
    successCallback = callback;
    checkFocusRingState();
  };

  FocusRingManager.setObserver((primary, preview) => {
    if (primary && primary instanceof BackButtonNode) {
      focusRingState['primary']['role'] = 'back';
      focusRingState['primary']['name'] = '';
    } else if (primary && primary.automationNode) {
      const node = primary.automationNode;
      focusRingState['primary']['role'] = node.role;
      focusRingState['primary']['name'] = node.name;
    } else {
      focusRingState['primary']['role'] = '';
      focusRingState['primary']['name'] = '';
    }
    if (preview && preview.automationNode) {
      const node = preview.automationNode;
      focusRingState['preview']['role'] = node.role;
      focusRingState['preview']['name'] = node.name;
    } else {
      focusRingState['preview']['role'] = '';
      focusRingState['preview']['name'] = '';
    }
    transcript.push(`Focus ring state: ${JSON.stringify(focusRingState)}`);
    checkFocusRingState();
  });
  window.domAutomationController.send('ready');

  setInterval(() => {
    console.error(
        'Test still running. Transcript so far:\n' + transcript.join('\n'));
  }, 5000);
})();
