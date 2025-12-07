// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Unit test for the Braille IME.
 */

GEN_INCLUDE(['../common/testing/e2e_test_base.js']);

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
 * Mock chrome.storage.local mv2, supports set and get.
 */
class MockLocalStorage {
  constructor() {
    this.store = {};
  }

  /**
   * Set the keyed storage.
   * @param {Object}
   */
  set(obj, callback) {
    for (let key in obj) {
      this.store[key] = obj[key];
    }
    callback();
  }

  /**
   * Gets the current storage. This function will only work with a callback
   * provided, like chrome.storage.local.set mv2.
   * @param {Array<string>} key for receiving specific store entries, ignored.
   * @param {function()}
   */
  get(keys, callback) {
    callback(this.store);
  }
}

/**
 * Engine ID as specified in manifest.
 * @const {string}
 */
const ENGINE_ID = 'braille';

/** Test fixture for the braille IME unit test. */
BrailleImeUnitTest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/common/chrome_paths.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/ime/ash/extension_ime_util.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    // Use a test extension to host this test suite.
    GEN(`
      base::FilePath resources_path;
      ASSERT_TRUE(base::PathService::Get(chrome::DIR_RESOURCES,
                                         &resources_path));

      // Randomly generated extension id and its manifest key. This is because
      // WaitForExtension need to pass in an extension id before it is loaded
      // so we could not rely on extension loader to generate a default one.
      const char kExtensionId[] = "kjaakbloiojfjkogmejphfpgejngoama";
      const char kKey[] =
          "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuEz2eIbHVJymg0lMnvs/kg"
          "2szbydMYeiEDW9L4A4wumUy63StCcxBswE4vfyz/pOAEZkkvQhExsSEhymFVhuqNTK"
          "jYCY/t4XwnF9CaMoeRnZgS/QvUCfb6Pr59jEj5x/sK5WeVI6Pc24INu2yOGE1wvNdU"
          "hhlN+j9isa205RWc6rWA6KMjiqjkkWNUt3TTPhlc+GsiZo69qHyhxirR3upNCyii4e"
          "TossXuD9yc1gMQ1Icv/JC+9h59gupFJ85xcIzbEQ4XypEfB5xNKGtAKiAPkk93beUN"
          "IXZ9wEw7E3c4VnW7qhl5NeHx0VWOLsTs27kmEgxhvgJOcAf//eIuIFkwIDAQAB";

      extensions::TestExtensionDir ext_dir;
      ext_dir.WriteManifest(base::StringPrintf(R"({
        "name": "Test",
        "version": "0.0.1",
        "manifest_version": 3,
        "key": "%s",
        "permissions": [
          "input",
          "storage"
        ],
        "background": {
          "service_worker": "sw.js",
          "type": "module"
        }
      })", kKey));

      ext_dir.CopyFileTo(
          resources_path.Append(
              FILE_PATH_LITERAL("chromeos/accessibility/common/testing/"
                                "test_import_manager.js")),
          FILE_PATH_LITERAL("test_import_manager.js"));

      const auto kBrailleIme = FILE_PATH_LITERAL("braille_ime.js");
      ext_dir.CopyFileTo(
          resources_path.Append(
              ash::extension_ime_util::kBrailleImeExtensionPath)
              .Append(kBrailleIme),
          kBrailleIme);

      ext_dir.WriteFile(
          "sw.js",
          R"(
            import {BrailleIme} from './braille_ime.js';
            import {TestImportManager} from './test_import_manager.js';

            globalThis.BrailleIme = BrailleIme;

            // Add a heartbeat function so that the service worker doesn't get
            // terminated in the middle of the test.
            setInterval(chrome.runtime.getPlatformInfo, 25 * 1000);
          )");
      extensions::ChromeTestExtensionLoader extension_loader(GetProfile());

      base::OnceClosure load_cb = base::BindLambdaForTesting([&] {
        ASSERT_TRUE(extension_loader.LoadExtension(ext_dir.UnpackedPath()));
      });
      WaitForExtension(kExtensionId, std::move(load_cb));

      bool fail_on_console_error = true;
      extensions::TestExtensionConsoleObserver
        console_observer(GetProfile(), kExtensionId, fail_on_console_error);
    `);
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // `BrailleIme` should be loaded in the test extension.
    assertNotNullNorUndefined(BrailleIme);

    chrome = chrome || {};
    chrome.input = chrome.input || {};
    chrome.input.ime = chrome.input.ime || {};
    chrome.runtime = chrome.runtime || {};
    chrome.storage = {local: new MockLocalStorage()};

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


AX_TEST_F('BrailleImeUnitTest', 'KeysWhenStandardKeyboardDisabled', function() {
  this.activateIme();
  assertFalse(this.sendKeyDown('KeyF'));
  assertFalse(this.sendKeyDown('KeyD'));
  assertFalse(this.sendKeyUp('KeyD'));
  assertFalse(this.sendKeyUp('KeyF'));
  assertEquals(0, this.port.messages.length);
});

AX_TEST_F('BrailleImeUnitTest', 'KeysWhenStandardKeysEnabled', function() {
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

AX_TEST_F('BrailleImeUnitTest', 'TestBackspaceKey', function() {
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

AX_TEST_F(
    'BrailleImeUnitTest', 'UseStandardKeyboardSettingPreserved', function() {
      this.activateIme();
      assertFalse(this.menuItems[0].checked);
      this.onMenuItemActivated.dispatch(ENGINE_ID, this.menuItems[0].id);
      assertTrue(this.menuItems[0].checked);
      // Create a new instance and make sure the setting is still turned on.
      this.createIme();
      this.activateIme();
      assertTrue(this.menuItems[0].checked);
    });

AX_TEST_F('BrailleImeUnitTest', 'ReplaceText', function() {
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

AX_TEST_F('BrailleImeUnitTest', 'Uncommitted', function() {
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
