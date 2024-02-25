// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Unit test for the Braille IME.
 */

/**
 * Mock Chrome event supporting one listener.
 */
class MockEvent {
  constructor() {
    /** @type {Function?} */
    this.listener = null;
  }

  /**
   * @param {Function} listener
   */
  addListener(listener) {
    assertTrue(this.listener === null);
    this.listener = listener;
  }

  /**
   * Dispatches an event to the listener if any.
   * @param {...*} var_args Arguments to pass to the event listener.
   * @return {*} Return value from listener or {@code undefined} if no
   *     listener.
   */
  dispatch() {
    if (this.listener) {
      return this.listener.apply(null, arguments);
    }
  }
}


/**
 * Mock port that supports the {@code onMessage} and {@code onDisconnect}
 * events as well as {@code postMessage}.
 */
class MockPort {
  constructor() {
    this.onMessage = new MockEvent();
    this.onDisconnect = new MockEvent();
    /** @type {Array<Object>} */
    this.messages = [];
  }

  /**
   * Stores {@code message} in this object.
   * @param {Object} message Message to store.
   */
  postMessage(message) {
    this.messages.push(message);
  }
}


/**
 * Engine ID as specified in manifest.
 * @const {string}
 */
ENGINE_ID = 'braille';

var localStorage;

/**
 * Test fixture for the braille IME unit test.
 */
BrailleImeUnitTest = class extends testing.Test {
  /** @override */
  setUp() {
    super.setUp();
    chrome = chrome || {};
    chrome.input = chrome.input || {};
    chrome.input.ime = chrome.input.ime || {};
    chrome.runtime = chrome.runtime || {};
    localStorage = {};
    this.lastSentKeyRequestId_ = 0;
    this.lastHandledKeyRequestId_ = undefined;
    this.lastHandledKeyResult_ = undefined;
    chrome.input.ime.keyEventHandled = function(requestId, result) {
      this.lastHandledKeyRequestId_ = Number(requestId);
      this.lastHandledKeyResult_ = result;
    }.bind(this);
    this.createIme();
  }

  createIme() {
    var IME_EVENTS = [
      'onActivate',
      'onDeactivated',
      'onFocus',
      'onBlur',
      'onInputContextUpdate',
      'onKeyEvent',
      'onReset',
      'onMenuItemActivated',
    ];
    for (var i = 0, name; name = IME_EVENTS[i]; ++i) {
      this[name] = chrome.input.ime[name] = new MockEvent();
    }
    chrome.input.ime.setMenuItems = function(parameters) {
      this.menuItems = parameters.items;
    }.bind(this);
    chrome.runtime.connect = function() {
      this.port = new MockPort();
      return this.port;
    }.bind(this);
    this.menuItems = null;
    this.port = null;
    this.ime = new BrailleIme();
    this.ime.init();
  }

  activateIme() {
    this.onActivate.dispatch(ENGINE_ID);
    assertDeepEquals(this.port.messages, [{type: 'activeState', active: true}]);
    this.port.messages.length = 0;
  }

  sendKeyEvent_(type, code, extra) {
    var event = {type, code, requestId: (++this.lastSentKeyRequestId_) + ''};
    for (var key in extra) {
      event[key] = extra[key];
    }
    this.onKeyEvent.dispatch(ENGINE_ID, event);
    if (this.lastSentKeyRequestId_ === this.lastHandledKeyRequestId_) {
      return this.lastHandledKeyResult_;
    }
  }

  sendKeyDown(code, extra) {
    return this.sendKeyEvent_('keydown', code, extra);
  }

  sendKeyUp(code, extra) {
    return this.sendKeyEvent_('keyup', code, extra);
  }
};

/** @Override */
BrailleImeUnitTest.prototype.extraLibraries = ['braille_ime.js'];


TEST_F('BrailleImeUnitTest', 'KeysWhenStandardKeyboardDisabled', function() {
  this.activateIme();
  assertFalse(this.sendKeyDown('KeyF'));
  assertFalse(this.sendKeyDown('KeyD'));
  assertFalse(this.sendKeyUp('KeyD'));
  assertFalse(this.sendKeyUp('KeyF'));
  assertEquals(0, this.port.messages.length);
});

TEST_F('BrailleImeUnitTest', 'KeysWhenStandardKeysEnabled', function() {
  this.activateIme();
  assertFalse(this.menuItems[0].checked);
  this.onMenuItemActivated.dispatch(ENGINE_ID, this.menuItems[0].id);
  assertTrue(this.menuItems[0].checked);
  // Type the letters 'b' and 'c' and verify the right dots get sent.
  assertTrue(this.sendKeyDown('KeyF'));
  assertTrue(this.sendKeyDown('KeyD'));
  assertTrue(this.sendKeyUp('KeyD'));
  assertTrue(this.sendKeyUp('KeyF'));
  assertTrue(this.sendKeyDown('KeyJ'));
  assertTrue(this.sendKeyDown('KeyF'));
  assertTrue(this.sendKeyUp('KeyJ'));
  assertTrue(this.sendKeyUp('KeyF'));

  // Make sure that other keys are not handled, either by themselves or while
  // one of the 'braille keys' is pressed.
  assertFalse(this.sendKeyDown('KeyX'));
  assertFalse(this.sendKeyUp('KeyX'));

  assertTrue(this.sendKeyDown('KeyS'));   // Dot 3
  assertFalse(this.sendKeyDown('KeyG'));  // To the right of dot 1.
  assertTrue(this.sendKeyUp('KeyS'));
  assertFalse(this.sendKeyUp('KeyG'));

  // Keys like Ctrl L should not be handled, despite L being a dot key.
  var ctrlFlag = {ctrlKey: true};
  assertFalse(this.sendKeyDown('ControlLeft', ctrlFlag));
  assertFalse(this.sendKeyDown('KeyL', ctrlFlag));
  assertFalse(this.sendKeyUp('KeyL', ctrlFlag));
  assertFalse(this.sendKeyUp('ControlLeft', ctrlFlag));

  // Space key by itself should send a blank cell.
  assertTrue(this.sendKeyDown('Space'));
  assertTrue(this.sendKeyUp('Space'));

  // Space and braille dots results in no event.
  assertTrue(this.sendKeyDown('Space'));
  assertTrue(this.sendKeyDown('KeyF'));
  assertTrue(this.sendKeyUp('Space'));
  assertTrue(this.sendKeyUp('KeyF'));
  // Send the braille key first, still no event should be produced.
  assertTrue(this.sendKeyDown('KeyF'));
  assertTrue(this.sendKeyDown('Space'));
  assertTrue(this.sendKeyUp('Space'));
  assertTrue(this.sendKeyUp('KeyF'));

  assertDeepEquals(this.port.messages, [
    {type: 'brailleDots', dots: 0x03},
    {type: 'brailleDots', dots: 0x09},
    {type: 'brailleDots', dots: 0},
  ]);
});

TEST_F('BrailleImeUnitTest', 'TestBackspaceKey', function() {
  this.activateIme();
  // Enable standard keyboard feature.
  assertFalse(this.menuItems[0].checked);
  this.onMenuItemActivated.dispatch(ENGINE_ID, this.menuItems[0].id);
  assertTrue(this.menuItems[0].checked);

  assertEquals(undefined, this.sendKeyDown('Backspace'));
  assertDeepEquals(
      this.port.messages,
      [{type: 'backspace', requestId: this.lastSentKeyRequestId_ + ''}]);
  this.port.onMessage.dispatch({
    type: 'keyEventHandled',
    requestId: this.lastSentKeyRequestId_ + '',
    result: true,
  });
  assertEquals(this.lastSentKeyRequestId_, this.lastHandledKeyRequestId_);
  assertTrue(this.lastHandledKeyResult_);
});

TEST_F('BrailleImeUnitTest', 'UseStandardKeyboardSettingPreserved', function() {
  this.activateIme();
  assertFalse(this.menuItems[0].checked);
  this.onMenuItemActivated.dispatch(ENGINE_ID, this.menuItems[0].id);
  assertTrue(this.menuItems[0].checked);
  // Create a new instance and make sure the setting is still turned on.
  this.createIme();
  this.activateIme();
  assertTrue(this.menuItems[0].checked);
});

TEST_F('BrailleImeUnitTest', 'ReplaceText', function() {
  var CONTEXT_ID = 1;
  var hasSelection = false;
  var text = 'Hi, ';
  chrome.input.ime.commitText = function(params) {
    assertEquals(CONTEXT_ID, params.contextID);
    text += params.text;
  };
  chrome.input.ime.deleteSurroundingText = function(params, callback) {
    assertEquals(ENGINE_ID, params.engineID);
    assertEquals(CONTEXT_ID, params.contextID);
    assertEquals(0, params.offset + params.length);
    if (hasSelection) {
      assertEquals(0, params.length);
      hasSelection = false;
    } else {
      text = text.slice(0, params.offset);
    }
    callback();
  };
  var sendReplaceText = function(deleteBefore, newText) {
    this.port.onMessage.dispatch(
        {type: 'replaceText', contextID: CONTEXT_ID, deleteBefore, newText});
  }.bind(this);
  this.activateIme();
  sendReplaceText(0, 'hello!');
  assertEquals('Hi, hello!', text);
  hasSelection = true;
  sendReplaceText('hello!'.length, 'good bye!');
  assertFalse(hasSelection);
  assertEquals('Hi, good bye!', text);
});

TEST_F('BrailleImeUnitTest', 'Uncommitted', function() {
  var CONTEXT_ID = 1;
  var text = '';
  chrome.input.ime.commitText = function(params) {
    assertEquals(CONTEXT_ID, params.contextID);
    text += params.text;
  };
  var sendSetUncommitted = function(text) {
    this.port.onMessage.dispatch(
        {type: 'setUncommitted', contextID: CONTEXT_ID, text});
  }.bind(this);
  var sendCommitUncommitted = function(contextID) {
    this.port.onMessage.dispatch({type: 'commitUncommitted', contextID});
  }.bind(this);

  this.activateIme();
  sendSetUncommitted('Hi');
  assertEquals('', text);
  sendSetUncommitted('Hello');
  sendCommitUncommitted(CONTEXT_ID);
  assertEquals('Hello', text);
  sendSetUncommitted(' there!');
  sendCommitUncommitted(CONTEXT_ID + 1);
  assertEquals('Hello', text);

  sendSetUncommitted(' you!');
  assertFalse(this.sendKeyDown('KeyY'));
  assertEquals('Hello you!', text);
  assertFalse(this.sendKeyUp('KeyY'));
  assertEquals('Hello you!', text);
});
