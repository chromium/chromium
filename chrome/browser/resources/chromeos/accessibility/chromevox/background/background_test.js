// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

GEN_INCLUDE([
  '../../common/testing/documents.js',
  '../testing/fake_objects.js',
]);

/**
 * Test fixture for Background.
 */
ChromeVoxBackgroundTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    globalThis.simulateHitTestResult = this.simulateHitTestResult;
    globalThis.press = this.press;
    globalThis.doDefault = this.doDefault;
    globalThis.focus = this.focus;

    globalThis.ActionType = chrome.automation.ActionType;
    globalThis.EventType = chrome.automation.EventType;
    globalThis.Gesture = chrome.accessibilityPrivate.Gesture;
    globalThis.Mod = constants.ModifierFlag;
    globalThis.RoleType = chrome.automation.RoleType;

    this.forceContextualLastOutput();
  }

  simulateHitTestResult(node) {
    return () => GestureCommandHandler.instance.pointerHandler_
                     .handleHitTestResult_(node);
  }

  press(keyCode, modifiers) {
    return function() {
      EventGenerator.sendKeyPress(keyCode, modifiers);
    };
  }

  focus(node) {
    return () => node.focus();
  }

  doDefault(node) {
    return () => node.doDefault();
  }


  get linksAndHeadingsDoc() {
    return `
      <p>start</p>
      <a href='#a'>alpha</a>
      <a href='#b'>beta</a>
      <p>
        <h1>charlie</h1>
        <a href='foo'>delta</a>
      </p>
      <a href='#bar'>echo</a>
      <h2>foxtraut</h2>
      <p>end<span>of test</span></p>
    `;
  }

  get buttonDoc() {
    return `
      <p>start</p>
      <button>hello button one</button>
      cats
      <button>hello button two</button>
      <p>end</p>
    `;
  }

  get formsDoc() {
    return `
      <select id="fruitSelect">
        <option>apple</option>
        <option>grape</option>
        <option> banana</option>
      </select>
    `;
  }

  get iframesDoc() {
    return `
      <p>start</p>
      <button>Before</button>
      <iframe srcdoc="<button>Inside</button><h1>Inside</h1>"></iframe>
      <button>After</button>
    `;
  }

  get disappearingObjectDoc() {
    return `
      <p>start</p>
      <div role="group">
        <p>Before1</p>
        <p>Before2</p>
        <p>Before3</p>
      </div>
      <div role="group">
        <p id="disappearing">Disappearing</p>
      </div>
      <div role="group">
        <p>After1</p>
        <p>After2</p>
        <p>After3</p>
      </div>
      <div id="live" aria-live="assertive"></div>
      <div id="delete" role="button">Delete</div>
      <script>
        document.getElementById('delete').addEventListener('click', function() {
          let d = document.getElementById('disappearing');
          d.parentElement.removeChild(d);
          document.getElementById('live').innerText = 'Deleted';
          document.body.offsetTop
        });
      </script>
    `;
  }

  get detailsDoc() {
    return `
      <p aria-details="details">start</p>
      <div role="group">
        <p>Before</p>
        <p id="details">Details</p>
        <p>After</p>
      </div>
    `;
  }

  get comboBoxDoc() {
    return `
      <div id="combo-box-label">Choose an item</div>
      <div aria-labelledby="combo-box-label" role="combobox">
        <input type="text" aria-controls="combo-box-list-box">
        <ul role="listbox" id="combo-box-list-box" hidden>
          <li role="option" tabindex="-1">Item 1</li>
          <li role="option" tabindex="-1">Item 2</li>
        </ul>
      </div>
    `;
  }

  get listBoxDoc() {
    return `
      <p>Start</p>
      <div role="listbox" aria-expanded="false" aria-label="Select an item">
        <div aria-selected="true" tabindex="0" role="option">
          <span>Listbox item one</span>
        </div>
        <div aria-selected="false" tabindex="-1" role="option">
          <span>Listbox item two</span>
        </div>
        <div aria-selected="false" role="option">
          <span>Listbox item three</span>
        </div>
      </div>
      <button>Click</button>
    `;
  }

  get nestedListDoc() {
    return `
      <div>
        <ul>
          <li>Lemons</li>
          <li>Oranges</li>
          <li>Berries
            <ul>
              <li>Strawberries</li>
              <li>Raspberries</li>
            </ul>
          </li>
          <li>Bananas</li>
        </ul>
      </div>
    `;
  }

  /**
   * Fires an onCustomSpokenFeedbackToggled event with enabled state of
   * |enabled|.
   * @param {boolean} enabled TalkBack-enabled state.
   */
  dispatchOnCustomSpokenFeedbackToggledEvent(enabled) {
    chrome.accessibilityPrivate.onCustomSpokenFeedbackToggled.dispatch(enabled);
  }
};

/**
 * Specific test fixture for tests that need a test server running.
 */
ChromeVoxBackgroundTestWithTestServer = class extends ChromeVoxBackgroundTest {
  get testServer() {
    return true;
  }
};

/** Tests that ChromeVox's background object is not available globally. */
AX_TEST_F('ChromeVoxBackgroundTest', 'NextNamespaces', function() {
  assertEquals(undefined, globalThis.Background);
});

/** Tests consistency of navigating forward and backward. */
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ForwardBackwardNavigation', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.linksAndHeadingsDoc);
      mockFeedback.expectSpeech('start').expectBraille('start');

      mockFeedback.call(doCmd('nextLink'))
          .expectSpeech('alpha', 'Link')
          .expectBraille('alpha lnk');
      mockFeedback.call(doCmd('nextLink'))
          .expectSpeech('beta', 'Link')
          .expectBraille('beta lnk');
      mockFeedback.call(doCmd('nextLink'))
          .expectSpeech('delta', 'Link')
          .expectBraille('delta lnk');
      mockFeedback.call(doCmd('previousLink'))
          .expectSpeech('beta', 'Link')
          .expectBraille('beta lnk');
      mockFeedback.call(doCmd('nextHeading'))
          .expectSpeech('charlie', 'Heading 1')
          .expectBraille('charlie h1');
      mockFeedback.call(doCmd('nextHeading'))
          .expectSpeech('foxtraut', 'Heading 2')
          .expectBraille('foxtraut h2');
      mockFeedback.call(doCmd('previousHeading'))
          .expectSpeech('charlie', 'Heading 1')
          .expectBraille('charlie h1');

      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('delta', 'Link')
          .expectBraille('delta lnk');
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('echo', 'Link')
          .expectBraille('echo lnk');
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('foxtraut', 'Heading 2')
          .expectBraille('foxtraut h2');
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('end')
          .expectBraille('end');
      mockFeedback.call(doCmd('previousObject'))
          .expectSpeech('foxtraut', 'Heading 2')
          .expectBraille('foxtraut h2');
      mockFeedback.call(doCmd('nextLine')).expectSpeech('foxtraut');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeech('end', 'of test')
          .expectBraille('endof test');

      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeech('start')
          .expectBraille('start');
      mockFeedback.call(doCmd('jumpToBottom'))
          .expectSpeech('of test')
          .expectBraille('of test');

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'CaretNavigation', async function() {
  // TODO(plundblad): Add braille expectations when crbug.com/523285 is fixed.
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree(this.linksAndHeadingsDoc);
  mockFeedback.expectSpeech('start');
  mockFeedback.call(doCmd('nextCharacter')).expectSpeech('t');
  mockFeedback.call(doCmd('nextCharacter')).expectSpeech('a');
  mockFeedback.call(doCmd('nextWord')).expectSpeech('alpha', 'Link');
  mockFeedback.call(doCmd('nextWord')).expectSpeech('beta', 'Link');
  mockFeedback.call(doCmd('previousWord')).expectSpeech('alpha', 'Link');
  mockFeedback.call(doCmd('nextWord')).expectSpeech('beta', 'Link');
  mockFeedback.call(doCmd('nextWord')).expectSpeech('charlie', 'Heading 1');
  mockFeedback.call(doCmd('nextLine')).expectSpeech('delta', 'Link');
  mockFeedback.call(doCmd('nextLine')).expectSpeech('echo', 'Link');
  mockFeedback.call(doCmd('nextLine')).expectSpeech('foxtraut', 'Heading 2');
  mockFeedback.call(doCmd('nextLine')).expectSpeech('end', 'of test');
  mockFeedback.call(doCmd('nextCharacter')).expectSpeech('n');
  mockFeedback.call(doCmd('previousCharacter')).expectSpeech('e');
  mockFeedback.call(doCmd('previousCharacter')).expectSpeech('t', 'Heading 2');
  mockFeedback.call(doCmd('previousWord')).expectSpeech('foxtraut');
  mockFeedback.call(doCmd('previousWord')).expectSpeech('echo', 'Link');
  mockFeedback.call(doCmd('previousCharacter')).expectSpeech('a', 'Link');
  mockFeedback.call(doCmd('previousCharacter')).expectSpeech('t');
  mockFeedback.call(doCmd('nextWord')).expectSpeech('echo', 'Link');
  await mockFeedback.replay();
});

/** Tests that individual buttons are stops for move-by-word functionality. */
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'CaretNavigationMoveThroughButtonByWord',
    async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.buttonDoc);
      mockFeedback.expectSpeech('start');
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('hello button one', 'Button');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('start');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('hello');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('button');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('one');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('cats');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('hello');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('button');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('two');
      mockFeedback.call(doCmd('nextWord')).expectSpeech('end');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('two');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('button');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('hello');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('cats');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('one');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('button');
      mockFeedback.call(doCmd('previousWord')).expectSpeech('hello');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'SelectSingleBasic', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.formsDoc);
      mockFeedback.expectSpeech('apple', 'has pop up', 'Collapsed')
          .expectBraille('apple btn +popup +3 +')
          .call(press(KeyCode.DOWN))
          .expectSpeech('grape', /2 of 3/)
          .expectBraille('grape 2/3')
          .call(press(KeyCode.DOWN))
          .expectSpeech('banana', /3 of 3/)
          .expectBraille('banana 3/3');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'ContinuousRead', async function() {
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree(this.linksAndHeadingsDoc);
  mockFeedback.expectSpeech('start')
      .call(doCmd('readFromHere'))
      .expectSpeech(
          'start', 'alpha', 'Link', 'beta', 'Link', 'charlie', 'Heading 1');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'InitialFocus', async function() {
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree('<a href="a">a</a>');
  mockFeedback.expectSpeech('a').expectSpeech('Link');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'AriaLabel', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = '<a aria-label="foo" href="a">a</a>';
  const rootNode = await this.runWithLoadedTree(site);
  rootNode.find({role: RoleType.LINK}).focus();
  mockFeedback.expectSpeech('foo')
      .expectSpeech('Link')
      .expectSpeech('Press Search+Space to activate')
      .expectBraille('foo lnk');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ShowContextMenu', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode =
      await this.runWithLoadedTree('<p>before</p><a href="a">a</a>');
  const go = rootNode.find({role: RoleType.LINK});
  mockFeedback.call(focus(go))
      .expectSpeech('a', 'Link')
      .call(doCmd('contextMenu'))
      .expectSpeech(/menu opened/)
      .call(press(KeyCode.ESCAPE))
      .expectSpeech('a', 'Link');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'BrailleRouting', async function() {
  const mockFeedback = this.createMockFeedback();
  const route = function(position) {
    assertTrue(ChromeVoxState.instance.onBrailleKeyEvent(
        {command: BrailleKeyCommand.ROUTING, displayPosition: position},
        mockFeedback.lastMatchedBraille));
  };
  const site = `
    <p>start</p>
    <button type="button" id="btn1">Click me</button>
    <p>Some text</p>
    <button type="button" id="btn2">Focus me</button>
    <p>Some more text</p>
    <input type="text" id ="text" value="Edit me">
    <script>
      document.getElementById('btn1').addEventListener('click', function() {
        document.getElementById('btn2').focus();
      }, false);
    </script>
  `;
  const rootNode = await this.runWithLoadedTree(site);
  const button1 =
      rootNode.find({role: RoleType.BUTTON, attributes: {name: 'Click me'}});
  const textField = rootNode.find({role: RoleType.TEXT_FIELD});
  mockFeedback.expectBraille('start')
      .call(focus(button1))
      .expectBraille(/^Click me btn/)
      .call(route.bind(null, 5))
      .expectBraille(/Focus me btn/)
      .call(focus(textField))
      .expectBraille('Edit me ed', {startIndex: 0})
      .call(route.bind(null, 3))
      .expectBraille('Edit me ed', {startIndex: 3})
      .call(function() {
        assertEquals(3, textField.textSelStart);
      });
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'FocusInputElement', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
      <input id="name" value="Lancelot">
      <input id="quest" value="Grail">
      <input id="color" value="Blue">
    `;
  const rootNode = await this.runWithLoadedTree(site);
  const name = rootNode.find({attributes: {value: 'Lancelot'}});
  const quest = rootNode.find({attributes: {value: 'Grail'}});
  const color = rootNode.find({attributes: {value: 'Blue'}});

  mockFeedback.call(focus(quest))
      .expectSpeech('Grail', 'Edit text')
      .call(focus(color))
      .expectSpeech('Blue', 'Edit text')
      .call(focus(name))
      .expectNextSpeechUtteranceIsNot('Blue')
      .expectSpeech('Lancelot', 'Edit text');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'UseEditableState', async function() {
  const site = `
      <input type="text"></input>
      <p tabindex=0>hi</p>
    `;
  const rootNode = await this.runWithLoadedTree(site);
  const nonEditable = rootNode.find({role: RoleType.PARAGRAPH});
  const editable = rootNode.find({role: RoleType.TEXT_FIELD});

  nonEditable.focus();
  await this.waitForEvent(nonEditable, 'focus');
  assertTrue(!DesktopAutomationInterface.instance.textEditHandler);

  editable.focus();
  await this.waitForEvent(editable, 'focus');
  assertNotNullNorUndefined(
      DesktopAutomationInterface.instance.textEditHandler);
});

AX_TEST_F('ChromeVoxBackgroundTest', 'EarconsForControls', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
      <p>Initial focus will be on something that's not a control.</p>
      <a href="#">MyLink</a>
      <button>MyButton</button>
      <input type=checkbox>
      <input type=checkbox checked>
      <input>
      <select multiple><option>1</option></select>
      <select><option>2</option></select>
      <input type=range value=5>
    `;
  const rootNode = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('MyLink')
      .expectEarcon(EarconId.LINK)
      .call(doCmd('nextObject'))
      .expectSpeech('MyButton')
      .expectEarcon(EarconId.BUTTON)
      .call(doCmd('nextObject'))
      .expectSpeech('Check box')
      .expectEarcon(EarconId.CHECK_OFF)
      .call(doCmd('nextObject'))
      .expectSpeech('Check box')
      .expectEarcon(EarconId.CHECK_ON)
      .call(doCmd('nextObject'))
      .expectSpeech('Edit text')
      .expectEarcon(EarconId.EDITABLE_TEXT)

      // Editable text Search re-mappings are in effect.
      .call(doCmd('toggleStickyMode'))
      .expectSpeech('Sticky mode enabled')
      .call(doCmd('nextObject'))
      .expectSpeech('List box')
      .expectEarcon(EarconId.LISTBOX)
      .call(doCmd('nextObject'))
      .expectSpeech('Button', 'has pop up')
      .expectEarcon(EarconId.POP_UP_BUTTON)
      .call(doCmd('nextObject'))
      .expectSpeech(/Slider/)
      .expectEarcon(EarconId.SLIDER);

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ShouldNotFocusIframe', async function() {
  const site = `
    <iframe tabindex=0 src="data:text/html,<p>Inside</p>"></iframe>
    <button>outside</button>
  `;
  const root = await this.runWithLoadedTree(site);
  const iframe = root.find({role: RoleType.IFRAME});
  const button = root.find({role: RoleType.BUTTON});

  assertEquals('iframe', iframe.role);
  assertEquals('button', button.role);

  let didFocus = false;
  iframe.addEventListener('focus', function() {
    didFocus = true;
  });
  ChromeVoxRange.instance.current_ = CursorRange.fromNode(button);
  doCmd('previousElement');
  assertFalse(didFocus);
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ShouldFocusLink', async function() {
  const site = `
    <div><a href="#">mylink</a></div>
    <button>after</button>
  `;
  const root = await this.runWithLoadedTree(site);
  const link = root.find({role: RoleType.LINK});
  const button = root.find({role: RoleType.BUTTON});

  assertEquals('link', link.role);
  assertEquals('button', button.role);

  const didFocus = false;
  link.addEventListener('focus', this.newCallback(function() {
    // Success
  }));
  ChromeVoxRange.instance.current_ = CursorRange.fromNode(button);
  doCmd('previousElement');
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NoisySlider', async function() {
  const mockFeedback = this.createMockFeedback();
  // Slider aria-valuetext must change otherwise blink suppresses event.
  const site = `
    <button id="go">go</button>
    <div id="slider" tabindex=0 role="slider"></div>
    <script>
      function update() {
        let s = document.getElementById('slider');
        s.setAttribute('aria-valuetext', '');
        s.setAttribute('aria-valuetext', 'noisy');
        setTimeout(update, 500);
      }
      update();
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  const go = root.find({role: RoleType.BUTTON});
  const slider = root.find({role: RoleType.SLIDER});
  mockFeedback.call(focus(go))
      .expectNextSpeechUtteranceIsNot('noisy')
      .call(focus(slider))
      .expectSpeech('noisy')
      .expectSpeech('noisy');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'Checkbox', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div id="go" role="checkbox">go</div>
    <script>
      let go = document.getElementById('go');
      let isChecked = true;
      go.addEventListener('click', function(e) {
        if (isChecked) {
          go.setAttribute('aria-checked', true);
        } else {
          go.removeAttribute('aria-checked');
        }
        isChecked = !isChecked;
      });
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  const cbx = root.find({role: RoleType.CHECK_BOX});
  mockFeedback.call(focus(cbx))
      .expectSpeech('go')
      .expectSpeech('Check box')
      .expectSpeech('Not checked')
      .call(doDefault(cbx))
      .expectSpeech('go')
      .expectSpeech('Check box')
      .expectSpeech('Checked')
      .call(doDefault(cbx))
      .expectSpeech('go')
      .expectSpeech('Check box')
      .expectSpeech('Not checked');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'MixedCheckbox', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = '<div id="go" role="checkbox" aria-checked="mixed">go</div>';
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('go', 'Check box', 'Partially checked');
  await mockFeedback.replay();
});

/** Tests navigating into and out of iframes using nextButton */
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ForwardNavigationThroughIframeButtons',
    async function() {
      const mockFeedback = this.createMockFeedback();

      let running = false;
      const runTestIfIframeIsLoaded = async rootNode => {
        if (running) {
          return;
        }

        // Return if the iframe hasn't loaded yet.
        const iframe = rootNode.find({role: RoleType.IFRAME});
        const childDoc = iframe.firstChild;
        if (!childDoc || childDoc.children.length === 0) {
          return;
        }

        running = true;
        const beforeButton =
            rootNode.find({role: RoleType.BUTTON, name: 'Before'});
        beforeButton.focus();
        mockFeedback.expectSpeech('Before', 'Button');
        mockFeedback.call(doCmd('nextButton')).expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('nextButton')).expectSpeech('After', 'Button');
        mockFeedback.call(doCmd('previousButton'))
            .expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('previousButton'))
            .expectSpeech('Before', 'Button');
        await mockFeedback.replay();
      };

      const rootNode = await this.runWithLoadedTree(this.iframesDoc);
      runTestIfIframeIsLoaded(rootNode);

      this.desktop_.addEventListener('loadComplete', function(evt) {
        runTestIfIframeIsLoaded(rootNode);
      }, true);
    });

/** Tests navigating into and out of iframes using nextObject */
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ForwardObjectNavigationThroughIframes',
    async function() {
      const mockFeedback = this.createMockFeedback();

      let running = false;
      const runTestIfIframeIsLoaded = async rootNode => {
        if (running) {
          return;
        }

        // Return if the iframe hasn't loaded yet.
        const iframe = rootNode.find({role: 'iframe'});
        const childDoc = iframe.firstChild;
        if (!childDoc || childDoc.children.length === 0) {
          return;
        }

        running = true;
        const suppressFocusActionOutput = function() {
          BaseAutomationHandler.announceActions = false;
        };
        const beforeButton =
            rootNode.find({role: RoleType.BUTTON, name: 'Before'});
        mockFeedback.call(focus(beforeButton))
            .expectSpeech('Before', 'Button')
            .call(suppressFocusActionOutput)
            .call(doCmd('nextObject'))
            .expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('nextObject'))
            .expectSpeech('Inside', 'Heading 1');
        mockFeedback.call(doCmd('nextObject')).expectSpeech('After', 'Button');
        mockFeedback.call(doCmd('previousObject'))
            .expectSpeech('Inside', 'Heading 1');
        mockFeedback.call(doCmd('previousObject'))
            .expectSpeech('Inside', 'Button');
        mockFeedback.call(doCmd('previousObject'))
            .expectSpeech('Before', 'Button');
        await mockFeedback.replay();
      };

      const rootNode = await this.runWithLoadedTree(this.iframesDoc);
      runTestIfIframeIsLoaded(rootNode);

      this.desktop_.addEventListener('loadComplete', function(evt) {
        runTestIfIframeIsLoaded(rootNode);
      }, true);
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'SelectOptionSelected', async function() {
  // Undoes the ChromeVoxE2E call setting this to true. The doDefault
  // action should always be read.
  BaseAutomationHandler.announceActions = false;
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <select>
      <option>apple
      <option>banana
      <option>grapefruit
    </select>
  `;
  const root = await this.runWithLoadedTree(site);
  const select = root.find({role: RoleType.COMBO_BOX_SELECT});
  const selectLastOption = () => {
    const options = select.findAll({role: RoleType.MENU_LIST_OPTION});
    options[options.length - 1].doDefault();
  };

  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('apple')
      .expectSpeech('Button')
      .expectSpeech('Collapsed')
      .expectSpeech('Press Search+Space to activate')
      .call(doDefault(select))
      .expectSpeech('apple')
      // TODO(crbug.com/260178552): flaky whether this is read as an expanded
      // button or as a list item 1 of 3. This is also flaky when using
      // ChromeVox. Accept either for now -- both convey the current selection.
      // .expectSpeech('Button')
      // .expectSpeech('Expanded')
      .call(selectLastOption)
      .expectNextSpeechUtteranceIsNot('apple')
      .expectSpeech('grapefruit');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ToggleButton', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div aria-pressed="mixed" role="button">boldface</div>
    <div aria-pressed="true" role="button">ok</div>
    <div aria-pressed="false" role="button">cancel</div>
    <div aria-pressed role="button">close</div>
  `;
  const root = await this.runWithLoadedTree(site);
  const move = doCmd('nextObject');
  mockFeedback.call(move)
      .expectSpeech('boldface')
      .expectSpeech('Toggle Button')
      .expectSpeech('Partially pressed')

      .call(move)
      .expectSpeech('ok')
      .expectSpeech('Toggle Button')
      .expectSpeech('Pressed')

      .call(move)
      .expectSpeech('cancel')
      .expectSpeech('Toggle Button')
      .expectSpeech('Not pressed')

      .call(move)
      .expectSpeech('close')
      .expectSpeech('Button');

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'EditText', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <input type="text"></input>
    <input role="combobox" type="text"></input>
  `;
  const root = await this.runWithLoadedTree(site);
  const nextEditText = doCmd('nextEditText');
  const previousEditText = doCmd('previousEditText');
  mockFeedback.call(nextEditText)
      .expectSpeech('Combo box')
      .call(previousEditText)
      .expectSpeech('Edit text');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ComboBox', async function() {
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree(this.comboBoxDoc);
  mockFeedback.expectSpeech('Edit text', 'Choose an item', 'Combo box');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'BackwardForwardSync', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div aria-label="Group" role="group" tabindex=0>
      <input type="text"></input>
    </div>
    <ul>
      <li tabindex=0>
        <button>ok</button>
      </li>
    </ul>
  `;
  const root = await this.runWithLoadedTree(site);
  const listItem = root.find({role: RoleType.LIST_ITEM});

  mockFeedback.call(focus(listItem))
      .expectSpeech('ok', 'List item')
      .call(this.doCmd('nextObject'))
      .expectSpeech('\u2022 ')  // bullet
      .call(this.doCmd('nextObject'))
      .expectSpeech('Button')
      .call(this.doCmd('previousObject'))
      .expectSpeech('\u2022 ')  // bullet
      .call(this.doCmd('previousObject'))
      .expectSpeech('List item')
      .call(this.doCmd('previousObject'))
      .expectSpeech('Edit text')
      .call(this.doCmd('previousObject'))
      .expectSpeech('Group');
  await mockFeedback.replay();
});

/** Tests that navigation works when the current object disappears. */
AX_TEST_F('ChromeVoxBackgroundTest', 'DisappearingObject', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode = await this.runWithLoadedTree(this.disappearingObjectDoc);
  const deleteButton =
      rootNode.find({role: RoleType.BUTTON, attributes: {name: 'Delete'}});
  mockFeedback.expectSpeech('start').expectBraille('start');

  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('Before1')
      .call(doCmd('nextObject'))
      .expectSpeech('Before2')
      .call(doCmd('nextObject'))
      .expectSpeech('Before3')
      .call(doCmd('nextObject'))
      .expectSpeech('Disappearing')
      .call(doDefault(deleteButton))
      .expectSpeech('Deleted')
      .call(doCmd('nextObject'))
      .expectSpeech('After1')
      .call(doCmd('nextObject'))
      .expectSpeech('After2')
      .call(doCmd('previousObject'))
      .expectSpeech('After1')
      .call(doCmd('dumpTree'));

  /*
      // This is broken by cl/1260523 making tree updating (more)
     asynchronous.
      // TODO(aboxhall/dtseng): Add a function to wait for next tree update?
      mockFeedback
          .call(doCmd('previousObject'))
          .expectSpeech('Before3');
  */

  await mockFeedback.replay();
});

/** Tests that focus jumps to details properly when indicated. */
AX_TEST_F('ChromeVoxBackgroundTest', 'JumpToDetails', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode = await this.runWithLoadedTree(this.detailsDoc);
  mockFeedback.call(doCmd('jumpToDetails')).expectSpeech('Details');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ButtonNameValueDescription', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = '<input type="submit" aria-label="foo" value="foo"></input>';
      const root = await this.runWithLoadedTree(site);
      const btn = root.find({role: RoleType.BUTTON});
      mockFeedback.call(focus(btn)).expectSpeech('foo').expectSpeech('Button');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'NameFromHeadingLink', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>before</p>
    <h1><a href="google.com">go</a><p>here</p></h1>
  `;
  const root = await this.runWithLoadedTree(site);
  const link = root.find({role: RoleType.LINK});
  mockFeedback.call(focus(link))
      .expectSpeech('go')
      .expectSpeech('Link')
      .expectSpeech('Heading 1');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'OptionChildIndexCount', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="listbox">
      <p>Fruits</p>
      <div role="option">apple</div>
      <div role="option">banana</div>
    </div>
  `;

  const root = await this.runWithLoadedTree(site);
  // Select first child of the list box, similar to what happens if
  // navigated by Tab.
  const firstChild = root.find({role: RoleType.PARAGRAPH});
  mockFeedback.call(() => ChromeVoxRange.set(CursorRange.fromNode(firstChild)))
      .call(doCmd('nextObject'))
      .expectSpeech('List box')
      .expectSpeech('Fruits')
      .call(doCmd('nextObject'))
      .expectSpeech('apple')
      .expectSpeech(' 1 of 2 ')
      .call(doCmd('nextObject'))
      .expectSpeech('banana')
      .expectSpeech(' 2 of 2 ');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ListMarkerIsIgnored', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<ul><li>apple</ul>');
  mockFeedback.call(doCmd('nextObject'))
      .expectNextSpeechUtteranceIsNot('listMarker')
      .expectSpeech('\u2022 apple');  // bullet apple
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'SymetricComplexHeading', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <h4><p>NW</p><p>NE</p></h4>
    <h4><p>SW</p><p>SE</p></h4>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.call(doCmd('nextHeading'))
          .expectNextSpeechUtteranceIsNot('NE')
          .expectSpeech('NW')
          .call(doCmd('previousHeading'))
          .expectNextSpeechUtteranceIsNot('NE')
          .expectSpeech('NW');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ContentEditableJumpSyncsRange',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>start</p>
    <div contenteditable>
      <h1>Top News</h1>
      <h1>Most Popular</h1>
      <h1>Sports</h1>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      const assertRangeHasText = (text) => () =>
          assertEquals(text, ChromeVoxRange.current.start.node.name);

      mockFeedback.call(doCmd('nextEditText'))
          .expectSpeech('Top News Most Popular Sports')
          .call(doCmd('nextHeading'))
          .expectSpeech('Top News')
          .call(assertRangeHasText('Top News'))
          .call(doCmd('nextHeading'))
          .expectSpeech('Most Popular')
          .call(assertRangeHasText('Most Popular'))
          .call(doCmd('nextHeading'))
          .expectSpeech('Sports')
          .call(assertRangeHasText('Sports'));
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'Selection', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>simple</p>
    <p>doc</p>
  `;
  const root = await this.runWithLoadedTree(site);
  // Fakes a toggleSelection command.
  root.addEventListener(EventType.DOCUMENT_SELECTION_CHANGED, function() {
    if (root.focusObject.name === 'simple' && root.focusOffset === 3) {
      CommandHandlerInterface.instance.onCommand('toggleSelection');
    }
  }, true);

  mockFeedback.call(doCmd('toggleSelection'))
      .expectSpeech('simple', 'selected')
      .call(doCmd('nextObject'))
      .expectSpeech('doc', 'selected')
      .call(doCmd('previousObject'))
      .expectSpeech('doc', 'unselected')
      .call(doCmd('nextCharacter'))
      .expectSpeech('i', 'selected')
      .call(doCmd('previousCharacter'))
      .expectSpeech('i', 'unselected')
      .call(doCmd('nextCharacter'))
      .call(doCmd('nextCharacter'))
      .expectSpeech('End selection', 'sim');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'BasicTableCommands', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
  <table border=1>
    <tr><td>name</td><td>title</td><td>address</td><td>phone</td></tr>
    <tr><td>Dan</td><td>Mr</td><td>666 Elm Street</td><td>212 222 5555</td></tr>
  </table>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextRow'))
      .expectSpeech('Dan', 'row 2 column 1')
      .call(doCmd('previousRow'))
      .expectSpeech('name', 'row 1 column 1')
      .call(doCmd('previousRow'))
      .expectSpeech('No cell above')
      .call(doCmd('nextCol'))
      .expectSpeech('title', 'row 1 column 2')
      .call(doCmd('nextRow'))
      .expectSpeech('Mr', 'row 2 column 2')
      .call(doCmd('previousRow'))
      .expectSpeech('title', 'row 1 column 2')
      .call(doCmd('nextCol'))
      .expectSpeech('address', 'row 1 column 3')
      .call(doCmd('nextCol'))
      .expectSpeech('phone', 'row 1 column 4')
      .call(doCmd('nextCol'))
      .expectSpeech('No cell right')
      .call(doCmd('previousRow'))
      .expectSpeech('No cell above')
      .call(doCmd('nextRow'))
      .expectSpeech('212 222 5555', 'row 2 column 4')
      .call(doCmd('nextRow'))
      .expectSpeech('No cell below')
      .call(doCmd('nextCol'))
      .expectSpeech('No cell right')
      .call(doCmd('previousCol'))
      .expectSpeech('666 Elm Street', 'row 2 column 3')
      .call(doCmd('previousCol'))
      .expectSpeech('Mr', 'row 2 column 2')

      .call(doCmd('goToRowLastCell'))
      .expectSpeech('212 222 5555', 'row 2 column 4')
      .call(doCmd('goToRowLastCell'))
      .expectSpeech('212 222 5555')
      .call(doCmd('goToRowFirstCell'))
      .expectSpeech('Dan', 'row 2 column 1')
      .call(doCmd('goToRowFirstCell'))
      .expectSpeech('Dan')

      .call(doCmd('goToColFirstCell'))
      .expectSpeech('name', 'row 1 column 1')
      .call(doCmd('goToColFirstCell'))
      .expectSpeech('name')
      .call(doCmd('goToColLastCell'))
      .expectSpeech('Dan', 'row 2 column 1')
      .call(doCmd('goToColLastCell'))
      .expectSpeech('Dan')

      .call(doCmd('goToLastCell'))
      .expectSpeech('212 222 5555', 'row 2 column 4')
      .call(doCmd('goToLastCell'))
      .expectSpeech('212 222 5555')
      .call(doCmd('goToFirstCell'))
      .expectSpeech('name', 'row 1 column 1')
      .call(doCmd('goToFirstCell'))
      .expectSpeech('name');

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'MissingTableCells', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
  <table border=1>
    <tr><td>a</td><td>b</td><td>c</td></tr>
    <tr><td>d</td><td>e</td></tr>
    <tr><td>f</td></tr>
  </table>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('goToRowLastCell'))
      .expectSpeech('c', 'row 1 column 3')
      .call(doCmd('goToRowLastCell'))
      .expectSpeech('c')
      .call(doCmd('goToRowFirstCell'))
      .expectSpeech('a', 'row 1 column 1')
      .call(doCmd('goToRowFirstCell'))
      .expectSpeech('a')

      .call(doCmd('nextCol'))
      .expectSpeech('b', 'row 1 column 2')

      .call(doCmd('goToColLastCell'))
      .expectSpeech('e', 'row 2 column 2')
      .call(doCmd('goToColLastCell'))
      .expectSpeech('e')
      .call(doCmd('goToColFirstCell'))
      .expectSpeech('b', 'row 1 column 2')
      .call(doCmd('goToColFirstCell'))
      .expectSpeech('b')

      .call(doCmd('goToFirstCell'))
      .expectSpeech('a', 'row 1 column 1')
      .call(doCmd('goToFirstCell'))
      .expectSpeech('a')
      .call(doCmd('goToLastCell'))
      .expectSpeech('f', 'row 3 column 1')
      .call(doCmd('goToLastCell'))
      .expectSpeech('f');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'DisabledState', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = '<button aria-disabled="true">ok</button>';
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('ok', 'Disabled', 'Button');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'HeadingLevels', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h1>1</h1><h2>2</h2><h3>3</h3><h4>4</h4><h5>5</h5><h6>6</h6>
  `;
  const root = await this.runWithLoadedTree(site);
  const makeLevelAssertions = function(level) {
    mockFeedback.call(doCmd('nextHeading' + level))
        .expectSpeech('Heading ' + level)
        .call(doCmd('nextHeading' + level))
        .expectEarcon(EarconId.WRAP)
        .call(doCmd('previousHeading' + level))
        .expectEarcon(EarconId.WRAP);
  };
  for (let i = 1; i <= 6; i++) {
    makeLevelAssertions(i);
  }
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'EditableNavigation', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable>this is a test</div>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('this is a test')
      .call(doCmd('nextObject'))
      .expectSpeech('this is a test')
      .call(doCmd('nextWord'))
      .expectSpeech('is')
      .call(doCmd('nextWord'))
      .expectSpeech('a')
      .call(doCmd('nextWord'))
      .expectSpeech('test');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NavigationMovesFocus', async function() {
  const site = `
    <p>start</p>
    <input type="text"></input>
  `;
  const root = await this.runWithLoadedTree(site);
  doCmd('nextEditText')();
  await this.waitForEvent(root.find({role: RoleType.TEXT_FIELD}), 'focus');
  const textField = ChromeVoxRange.current.start.node;
  assertEquals(RoleType.TEXT_FIELD, textField.role);
  assertTrue(textField.state.focused);
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'BrailleCaretNavigation', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>This is a<em>test</em> of inline braille<br>with a second line</p>
  `;
      const root = await this.runWithLoadedTree(site);
      const text = 'This is a';
      mockFeedback.call(doCmd('nextCharacter'))
          .expectBraille(text, {startIndex: 1, endIndex: 2})  // h
          .call(doCmd('nextCharacter'))
          .expectBraille(text, {startIndex: 2, endIndex: 3})  // i
          .call(doCmd('nextWord'))
          .expectBraille(text, {startIndex: 5, endIndex: 7})  // is
          .call(doCmd('previousWord'))
          .expectBraille(text, {startIndex: 0, endIndex: 4})  // This
          .call(doCmd('nextLine'))
          // Ensure nothing is selected when the range covers the entire line.
          .expectBraille('with a second line', {startIndex: -1, endIndex: -1});
      await mockFeedback.replay();
    });

// This tests ChromeVox's special support for following an in-page link
// if you force-click on it. Compare with InPageLinks, below.
AX_TEST_F('ChromeVoxBackgroundTest', 'ForceClickInPageLinks', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <a href="#there">hi</a>
    <button id="there">there</button>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('hi', 'Internal link')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech('there', 'Button');
  await mockFeedback.replay();
});

// This tests ChromeVox's handling of the scrolledToAnchor event, which is
// fired when the users follows an in-page link or the document otherwise
// gets navigated to an in-page link target by the url fragment changing,
// not necessarily due to directly clicking on the link via ChromeVox.
//
// Note: this test needs the test server running because the browser
// does not follow same-page links on data urls (because it modifies the
// url fragment, and any change to the url is disallowed for a data url).
AX_TEST_F(
    'ChromeVoxBackgroundTestWithTestServer', 'InPageLinks', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(undefined, {
        url: `${
            testRunnerParams
                .testServerBaseUrl}accessibility/in_page_links.html`,
      });
      const link = root.find({role: RoleType.LINK});
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('Jump', 'Internal link')
          // Use doDefault instead of press(KeyCode.RETURN) to avoid flakes.
          .call(doDefault(link))
          .expectSpeech('Found It')
          .call(doCmd('nextHeading'))
          .expectSpeech('Continue Here', 'Heading 2');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'ListItem', async function() {
  this.resetContextualOutput();
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <ul><li>apple<li>grape<li>banana</ul>
    <ol><li>pork<li>beef<li>chicken</ol>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextLine'))
      .expectSpeech('\u2022 apple', 'List item')
      .expectBraille('\u2022 apple lstitm lst +3')
      .call(doCmd('nextLine'))
      .expectSpeech('\u2022 grape', 'List item')
      .expectBraille('\u2022 grape lstitm')
      .call(doCmd('nextLine'))
      .expectSpeech('\u2022 banana', 'List item')
      .expectBraille('\u2022 banana lstitm lst end')

      // Object nav should be the same.
      .call(doCmd('nextObject'))
      .expectSpeech('1. pork', 'List item')
      .expectBraille('1. pork lstitm lst +3')
      .call(doCmd('nextObject'))
      .expectSpeech('2. beef', 'List item')
      .expectBraille('2. beef lstitm')

      // Mixing with line nav.
      .call(doCmd('nextLine'))
      .expectSpeech('3. chicken', 'List item')
      .expectBraille('3. chicken lstitm lst end');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'BusyHeading', async function() {
  this.resetContextualOutput();
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <h2><a href="#">Lots</a><a href="#">going</a><a href="#">here</a></h2>
  `;
  const root = await this.runWithLoadedTree(site);
  // In the past, this would have inserted the 'heading 2' after the first
  // link's output. Make sure it goes to the end.
  mockFeedback.call(doCmd('nextLine'))
      .expectSpeech(
          'Lots', 'Link', 'going', 'Link', 'here', 'Link', 'Heading 2')
      .expectBraille('Lots lnk going lnk here lnk h2');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NodeVsSubnode', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<a href="#">test</a>');
  const link = root.find({role: RoleType.LINK});
  function outputLinkRange(start, end) {
    return function() {
      new Output()
          .withSpeech(
              new CursorRange(new Cursor(link, start), new Cursor(link, end)))
          .go();
    };
  }

  mockFeedback.call(outputLinkRange(0, 0))
      .expectSpeech('test', 'Internal link')
      .call(outputLinkRange(0, 1))
      .expectSpeech('t')
      .call(outputLinkRange(1, 1))
      .expectSpeech('test', 'Internal link')
      .call(outputLinkRange(1, 2))
      .expectSpeech('e')
      .call(outputLinkRange(1, 3))
      .expectNextSpeechUtteranceIsNot('Internal link')
      .expectSpeech('es')
      .call(outputLinkRange(0, 4))
      .expectSpeech('test', 'Internal link');
  await mockFeedback.replay();
});

// TODO(crbug.com/1419811): Flaky.
AX_TEST_F('ChromeVoxBackgroundTest', 'DISABLED_NativeFind', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <a href="#">grape</a>
    <a href="#">pineapple</a>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(press(KeyCode.F, {ctrl: true}))
      .expectSpeech('Find', 'Edit text')
      .call(press(KeyCode.G))
      .expectSpeech('grape', 'Link')
      .call(press(KeyCode.BACK))
      .call(press(KeyCode.L))
      .expectSpeech('pineapple', 'Link');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'EditableKeyCommand', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <input type="text"></input>
    <textarea>test</textarea>
    <div role="textbox" contenteditable>test</div>
  `;
  const root = await this.runWithLoadedTree(site);
  const assertCurNode = function(node) {
    return function() {
      assertEquals(node, ChromeVoxRange.current.start.node);
    };
  };

  const textField = root.firstChild;
  const textArea = textField.nextSibling;
  const contentEditable = textArea.nextSibling;

  mockFeedback.call(assertCurNode(textField))
      .call(doCmd('nextObject'))
      .call(assertCurNode(textArea))
      .call(doCmd('nextObject'))
      .call(assertCurNode(contentEditable))
      .call(doCmd('previousObject'))
      .expectSpeech('Text area')
      .call(assertCurNode(textArea))
      .call(doCmd('previousObject'))
      .call(assertCurNode(textField));

  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'TextSelectionAndLiveRegion', async function() {
      BaseAutomationHandler.announceActions = true;
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
  <p>start</p>
  <div><input value="test" type="text"></input></div>
  <div id="live" aria-live="assertive"></div>
  <script>
    const input = document.querySelector('input');
    const [div, live] = document.querySelectorAll('div');
    let clicks = 0;
    div.addEventListener('click', function() {
      clicks++;
      if (clicks === 1) {
        live.textContent = 'go';
      } else if (clicks === 2) {
        input.selectionStart = 1;
        live.textContent = 'queued';
      } else {
        input.selectionStart = 2;
        live.textContent = 'interrupted';
      }
    });
  </script>
      `);
      const textField = root.find({role: RoleType.TEXT_FIELD});
      const div = textField.parent;
      mockFeedback.call(focus(textField))
          .expectSpeech('Edit text')
          .call(doDefault(div))
          .expectSpeechWithQueueMode('go', QueueMode.CATEGORY_FLUSH)

          .call(doDefault(div))
          .expectSpeechWithQueueMode('queued', QueueMode.QUEUE)
          .expectSpeechWithQueueMode('e', QueueMode.CATEGORY_FLUSH)

          .call(doDefault(div))
          .expectSpeechWithQueueMode('interrupted', QueueMode.QUEUE)
          .expectSpeechWithQueueMode('s', QueueMode.CATEGORY_FLUSH);

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'TableColumnHeaders', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="grid">
      <div role="rowgroup">
        <div role="row">
          <div role="columnheader">city</div>
          <div role="columnheader">state</div>
          <div role="columnheader">zip</div>
        </div>
      </div>
      <div role="rowgroup">
        <div role="row">
          <div role="gridcell">Mountain View</div>
          <div role="gridcell">CA</div>
          <div role="gridcell">94043</div>
        </div>
        <div role="row">
          <div role="gridcell">San Jose</div>
          <div role="gridcell">CA</div>
          <div role="gridcell">95128</div>
        </div>
      </div>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextRow'))
      .expectSpeech('Mountain View', 'row 2 column 1')
      .call(doCmd('nextRow'))
      .expectNextSpeechUtteranceIsNot('city')
      .expectSpeech('San Jose', 'row 3 column 1')
      .call(doCmd('nextCol'))
      .expectSpeech('CA', 'row 3 column 2', 'state')
      .call(doCmd('previousRow'))
      .expectSpeech('CA', 'row 2 column 2')
      .call(doCmd('previousRow'))
      .expectSpeech('state', 'row 1 column 2');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ActiveDescendantUpdates', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div aria-label="container" tabindex=0 role="group" id="active"
        aria-activedescendant="1">
      <div id="1" role="treeitem" aria-selected="false"></div>
      <div id="2" role="treeitem" aria-selected="true"></div>
    <script>
      let alt = false;
      let active = document.getElementById('active');
      let one = document.getElementById('1');
      let two = document.getElementById('2');
      active.addEventListener('click', function() {
        let descendant = alt ? one : two;
        active.setAttribute('aria-activedescendant', descendant.id);
        alt = !alt;
      });
      </script>
  `;
      const root = await this.runWithLoadedTree(site);
      const group = root.firstChild;
      mockFeedback.call(focus(group))
          .call(
              () => assertTrue(RectUtil.equal(
                  FocusBounds.get()[0], group.firstChild.location)))
          .call(doDefault(group))
          .expectSpeech('Tree item', ' 2 of 2 ')
          .call(doDefault(group))
          .expectSpeech('Tree item', 'Not selected', ' 1 of 2 ');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'NavigationEscapesEdit', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>before content editable</p>
    <div role="textbox" contenteditable>this<br>is<br>a<br>test</div>
    <p>after content editable, before text area</p>
    <textarea style="word-spacing: 1000px">this is a test</textarea>
    <p>after text area</p>
  `;
  const root = await this.runWithLoadedTree(site);
  const assertBeginning = function(expected) {
    const textEditHandler = DesktopAutomationInterface.instance.textEditHandler;
    assertNotNullNorUndefined(textEditHandler);
    assertEquals(expected, textEditHandler.isSelectionOnFirstLine());
  };
  const assertEnd = function(expected) {
    const textEditHandler = DesktopAutomationInterface.instance.textEditHandler;
    assertNotNullNorUndefined(textEditHandler);
    assertEquals(expected, textEditHandler.isSelectionOnLastLine());
  };
  const [contentEditable, textArea] = root.findAll({role: RoleType.TEXT_FIELD});

  contentEditable.focus();
  await this.waitForEvent(contentEditable, EventType.FOCUS);
  mockFeedback.call(() => assertBeginning(true))
      .call(() => assertEnd(false))

      .call(press(KeyCode.DOWN))
      .expectSpeech('is')
      .call(() => assertBeginning(false))
      .call(() => assertEnd(false))

      .call(press(KeyCode.DOWN))
      .expectSpeech('a')
      .call(() => assertBeginning(false))
      .call(() => assertEnd(false))

      .call(press(KeyCode.DOWN))
      .expectSpeech('test')
      .call(() => assertBeginning(false))
      .call(() => assertEnd(true))

      .call(focus(textArea))
      .expectSpeech('Text area')
      .call(() => assertBeginning(true))
      .call(() => assertEnd(false))

      .call(press(40 /* ArrowDown */))
      .expectSpeech('is')
      .call(() => assertBeginning(false))
      .call(() => assertEnd(false))

      .call(press(40 /* ArrowDown */))
      .expectSpeech('a')
      .call(() => assertBeginning(false))
      .call(() => assertEnd(false))

      .call(press(40 /* ArrowDown */))
      .expectSpeech('test')
      .call(() => assertBeginning(false))
      .call(() => assertEnd(true));

  await mockFeedback.replay();

  // TODO: soft line breaks currently won't work in <textarea>.
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'DISABLED_SelectDoesNotSyncNavigation',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <select>
      <option>apple</option>
      <option>grape</option>
    </select>
  `;
      const root = await this.runWithLoadedTree(site);
      const select = root.find({role: RoleType.COMBO_BOX_SELECT});
      mockFeedback.expectSpeech('Button', 'has pop up', 'Collapsed')
          .call(doDefault(select))
          .expectSpeech('Expanded')
          .call(() => assertEquals(select, ChromeVoxRange.current.start.node))
          .call(press(KeyCode.DOWN))
          .expectSpeech('grape', 'List item', ' 2 of 2 ')
          .call(() => assertEquals(select, ChromeVoxRange.current.start.node))
          .call(press(KeyCode.UP))
          .expectSpeech('apple', 'List item', ' 1 of 2 ')
          .call(() => assertEquals(select, ChromeVoxRange.current.start.node));
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NavigationIgnoresLabels', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>before</p>
    <p id="label">label</p>
    <a href="#next" id="lebal">lebal</a>
    <h2 id="headingLabel">headingLabel</h2>
    <p>after</p>
    <button aria-labelledby="label headingLabel"></button>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('before')
          .call(doCmd('nextObject'))
          .expectSpeech('lebal', 'Link')
          .call(doCmd('nextObject'))
          .expectSpeech('headingLabel', 'Heading 2')
          .call(doCmd('nextObject'))
          .expectSpeech('after')
          .call(doCmd('previousObject'))
          .expectSpeech('headingLabel', 'Heading 2')
          .call(doCmd('previousObject'))
          .expectSpeech('lebal', 'Link')
          .call(doCmd('previousObject'))
          .expectSpeech('before')
          .call(doCmd('nextObject'))
          .expectSpeech('lebal', 'Link')
          .call(doCmd('nextObject'))
          .expectSpeech('headingLabel', 'Heading 2')
          .call(doCmd('nextObject'))
          .expectSpeech('after')
          .call(doCmd('nextObject'))
          .expectSpeech('label headingLabel', 'Button')
          .call(doCmd('nextObject'))
          .expectEarcon(EarconId.WRAP)
          .call(doCmd('nextObject'))
          .expectSpeech('before');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NavigationIgnoresDescriptions',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>before</p>
    <p id="desc">label</p>
    <a href="#next" id="csed">lebal</a>
    <p>after</p>
    <button aria-describedby="desc"></button>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('before')
          .call(doCmd('nextObject'))
          .expectSpeech('lebal', 'Link')
          .call(doCmd('nextObject'))
          .expectSpeech('after')
          .call(doCmd('previousObject'))
          .expectSpeech('lebal', 'Link')
          .call(doCmd('previousObject'))
          .expectSpeech('before')
          .call(doCmd('nextObject'))
          .expectSpeech('lebal', 'Link')
          .call(doCmd('nextObject'))
          .expectSpeech('after')
          .call(doCmd('nextObject'))
          .expectSpeech('label', 'lebal', 'Button')
          .call(doCmd('nextObject'))
          .expectEarcon(EarconId.WRAP)
          .call(doCmd('nextObject'))
          .expectSpeech('before');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'MathContentViaInnerHtml', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div role="math">
      <semantics>
        <mrow class="MJX-TeXAtom-ORD">
          <mstyle displaystyle="true" scriptlevel="0">
            <mi>a</mi>
            <mo stretchy="false">(</mo>
            <mi>y</mi>
            <mo>+</mo>
            <mi>m</mi>
            <msup>
              <mo stretchy="false">)</mo>
              <mrow class="MJX-TeXAtom-ORD">
                <mn>2</mn>
              </mrow>
            </msup>
            <mo>+</mo>
            <mi>b</mi>
            <mo stretchy="false">(</mo>
            <mi>y</mi>
            <mo>+</mo>
            <mi>m</mi>
            <mo stretchy="false">)</mo>
            <mo>+</mo>
            <mi>c</mi>
            <mo>=</mo>
            <mn>0.</mn>
          </mstyle>
        </mrow>
        <annotation encoding="application/x-tex">{\displaystyle a(y+m)^{2}+b(y+m)+c=0.}</annotation>
      </semantics>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('a ( y + m ) squared + b ( y + m ) + c = 0 .')
          .expectSpeech('Press up, down, left, or right to explore math');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'GestureGranularity', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>This is a test</p>
    <h2>hello</h2>
    <a href="#">greetings</a>
    <h2>here</h2>
    <button>and</button>
    <a href="#">there</a>
    <button>world</button>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doGesture(Gesture.SWIPE_LEFT3))
      .expectSpeech('Word')
      .call(doGesture(Gesture.SWIPE_DOWN1))
      .expectSpeech('is')
      .call(doGesture(Gesture.SWIPE_DOWN1))
      .expectSpeech('a')
      .call(doGesture(Gesture.SWIPE_UP1))
      .expectSpeech('is')

      .call(doGesture(Gesture.SWIPE_LEFT3))
      .expectSpeech('Character')
      .call(doGesture(Gesture.SWIPE_DOWN1))
      .expectSpeech('s')
      .call(doGesture(Gesture.SWIPE_UP1))
      .expectSpeech('i')

      .call(doGesture(Gesture.SWIPE_LEFT3))
      .expectSpeech('Form field control')
      .call(doGesture(Gesture.SWIPE_DOWN1))
      .expectSpeech('and', 'Button')
      .call(doGesture(Gesture.SWIPE_UP1))
      .expectSpeech('world', 'Button')

      .call(doGesture(Gesture.SWIPE_LEFT3))
      .expectSpeech('Link')
      .call(doGesture(Gesture.SWIPE_DOWN1))
      .expectSpeech('greetings', 'Internal link')
      .call(doGesture(Gesture.SWIPE_UP1))
      .expectSpeech('there', 'Internal link')

      .call(doGesture(Gesture.SWIPE_LEFT3))
      .expectSpeech('Heading')
      .call(doGesture(Gesture.SWIPE_DOWN1))
      .expectSpeech('hello', 'Heading 2')
      .call(doGesture(Gesture.SWIPE_UP1))
      .expectSpeech('here', 'Heading 2')
      .call(doGesture(Gesture.SWIPE_UP1))
      .expectSpeech('hello', 'Heading 2')

      .call(doGesture(Gesture.SWIPE_LEFT3))
      .expectSpeech('Line')
      .call(doGesture(Gesture.SWIPE_UP1))
      .expectSpeech('This is a test')

      .call(doGesture(Gesture.SWIPE_RIGHT3))
      .expectSpeech('Heading')
      .call(doGesture(Gesture.SWIPE_RIGHT3))
      .expectSpeech('Internal link')
      .call(doGesture(Gesture.SWIPE_RIGHT3))
      .expectSpeech('Form field control')
      .call(doGesture(Gesture.SWIPE_RIGHT3))
      .expectSpeech('Character');

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'LinesFilterWhitespace', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role="list">
      <div role="listitem">
        <span>Munich</span>
        <span>London</span>
      </div>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('start')
      .clearPendingOutput()
      .call(doCmd('nextLine'))
      .expectSpeech('Munich')
      .expectNextSpeechUtteranceIsNot(' ')
      .expectSpeech('London');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'TabSwitchAndRefreshRecovery', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTabs(['<p>tab1</p>', '<p>tab2</p>']);
      mockFeedback.expectSpeech('tab2')
          .clearPendingOutput()
          .call(press(KeyCode.TAB, {shift: true, ctrl: true}))
          .expectSpeech('tab1')
          .clearPendingOutput()
          .call(press(KeyCode.TAB, {ctrl: true}))
          .expectSpeech('tab2')
          .clearPendingOutput()
          .call(press(KeyCode.R, {ctrl: true}))

          // ChromeVox stays on the same node due to tree path recovery.
          .call(() => {
            assertEquals('tab2', ChromeVoxRange.current.start.node.name);
          });
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'ListName', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div id="_md-chips-wrapper-76" tabindex="-1" class="md-chips md-readonly"
        aria-setsize="4" aria-label="Favorite Sports" role="list"
        aria-describedby="chipsNote">
      <div role="listitem">Baseball</div>
      <div role="listitem">Hockey</div>
      <div role="listitem">Lacrosse</div>
      <div role="listitem">Football</div>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('Favorite Sports');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'LayoutTable', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <table><tr><td>start</td></tr></table><p>end</p>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('start')
      .call(doCmd('nextObject'))
      .expectNextSpeechUtteranceIsNot('row 1 column 1')
      .expectNextSpeechUtteranceIsNot('Table , 1 by 1')
      .expectSpeech('end');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ReinsertedNodeRecovery', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div>
      <button id="start">start</button>
      <button id="hot">hot</button>
    </div>
    <button id="end">end</button>
    <script>
      let div =       document.body.firstElementChild;
      let start =       document.getElementById('start');
      document.getElementById('hot').addEventListener('focus', evt => {
        let hot = evt.target;
        hot.remove();
        div.insertAfter(hot, start);
      });
    </script>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('start')
          .clearPendingOutput()
          .call(doCmd('nextObject'))
          .call(doCmd('nextObject'))
          .call(doCmd('nextObject'))
          .expectSpeech('end', 'Button');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'PointerTargetsLeafNode', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div role=button><p>Washington</p></div>
    <div role=button><p>Adams</p></div>
    <div role=button><p>Jefferson</p></div>
  `;
      const root = await this.runWithLoadedTree(site);
      const button =
          root.find({role: RoleType.BUTTON, attributes: {name: 'Jefferson'}});
      const buttonP = button.firstChild;
      assertNotNullNorUndefined(buttonP);
      const buttonText = buttonP.firstChild;
      assertNotNullNorUndefined(buttonText);
      mockFeedback.call(simulateHitTestResult(buttonText))
          .expectSpeech('Jefferson')
          .expectSpeech('Button');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'AriaSliderWithValueNow', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div id="slider" role="slider" tabindex="0" aria-valuemin="0"
             aria-valuenow="50" aria-valuemax="100"></div>
    <script>
      let slider = document.getElementById('slider');
      slider.addEventListener('click', () => {
        slider.setAttribute('aria-valuenow',
            parseInt(slider.getAttribute('aria-valuenow'), 10) + 1);
      });
    </script>
  `;
      const root = await this.runWithLoadedTree(site);
      const slider = root.find({role: RoleType.SLIDER});
      assertNotNullNorUndefined(slider);
      mockFeedback.call(doDefault(slider)).expectSpeech('51');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'AriaSliderWithValueText', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div id="slider" role="slider" tabindex="0" aria-valuemin="0"
             aria-valuenow="50" aria-valuemax="100" aria-valuetext="tiny"></div>
    <script>
      let slider = document.getElementById('slider');
      slider.addEventListener('click', () => {
        slider.setAttribute('aria-valuenow',
            parseInt(slider.getAttribute('aria-valuenow'), 10) + 1);
        slider.setAttribute('aria-valuetext', 'large');
      });
    </script>
  `;
      const root = await this.runWithLoadedTree(site);
      const slider = root.find({role: RoleType.SLIDER});
      assertNotNullNorUndefined(slider);
      mockFeedback.clearPendingOutput()
          .call(doDefault(slider))
          .expectNextSpeechUtteranceIsNot('51')
          .expectSpeech('large');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'SelectValidityOutput', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <label for="in1">Name:</label>
    <input id="in1" required>
    <script>
      const in1 = document.querySelector('input');
      in1.addEventListener('focus', () => {
        setTimeout(() => {
          in1.setCustomValidity('Please enter name');
          in1.reportValidity();
        }, 500);
      });
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('start')
      .call(doCmd('nextObject'))
      .expectSpeech('Name:')
      .expectSpeech('Edit text')
      .expectSpeech('Required')
      .expectNextSpeechUtteranceIsNot('Alert')
      .expectSpeech('Please enter name');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'EventFromAction', async function() {
  const site = '<button>ok</button><button>cancel</button>';
  const root = await this.runWithLoadedTree(site);
  const button = root.findAll({role: RoleType.BUTTON})[1];
  button.addEventListener(EventType.FOCUS, this.newCallback(function(evt) {
    assertEquals(RoleType.BUTTON, evt.target.role);
    assertEquals('action', evt.eventFrom);
    assertEquals('cancel', evt.target.name);
    assertEquals('focus', evt.eventFromAction);
  }));

  button.focus();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'EventFromUser', async function() {
  const site = '<button>ok</button><button>cancel</button>';
  const root = await this.runWithLoadedTree(site);
  const buttons = root.findAll({role: RoleType.BUTTON});
  const okButton = buttons[0];
  const cancelButton = buttons[1];

  await new Promise(r => {
    if (okButton.state.focused) {
      r();
    } else {
      okButton.addEventListener('focus', r);
    }
  });

  press(KeyCode.TAB)();

  const evt = await new Promise(r => cancelButton.addEventListener('focus', r));
  assertEquals(RoleType.BUTTON, evt.target.role);
  assertEquals('user', evt.eventFrom);
  assertEquals('cancel', evt.target.name);
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ReadPhoneticPronunciationTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
   <button>This is a button</button>
   <input type="text"></input>
  `;
      const root = await this.runWithLoadedTree(site);
      root.find({role: RoleType.BUTTON}).focus();
      mockFeedback.call(doCmd('readPhoneticPronunciation'))
          .expectSpeech(
              'T: tango, h: hotel, i: india, s: sierra,  : , i: india, ' +
              's: sierra,  : , a: alpha,  : , b: bravo, u: uniform, t: tango, ' +
              't: tango, o: oscar, n: november')
          .call(doCmd('nextWord'))
          .call(doCmd('readPhoneticPronunciation'))
          .expectSpeech('i: india, s: sierra')
          .call(doCmd('previousWord'))
          .call(doCmd('readPhoneticPronunciation'))
          .expectSpeech('T: tango, h: hotel, i: india, s: sierra')
          .call(doCmd('nextWord'))
          .call(doCmd('nextWord'))
          .call(doCmd('nextWord'))
          .call(doCmd('readPhoneticPronunciation'))
          .expectSpeech(
              'b: bravo, u: uniform, t: tango, t: tango, o: oscar, ' +
              'n: november')
          .call(doCmd('nextEditText'))
          .call(doCmd('readPhoneticPronunciation'))
          .expectSpeech('No available text for this item');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'SimilarItemNavigation', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h3><a href="#a">inner</a></h3>
    <p>some text</p>
    <button>some other text</button>
    <a href="#b">outer1</a>
    <h3>outer2</h3>
  `;
  const root = await this.runWithLoadedTree(site);
  assertEquals(RoleType.LINK, ChromeVoxRange.current.start.node.role);
  assertEquals('inner', ChromeVoxRange.current.start.node.name);
  mockFeedback.call(doCmd('nextSimilarItem'))
      .expectSpeech('outer1', 'Link')
      .call(doCmd('nextSimilarItem'))
      .expectSpeech('inner', 'Link')
      .call(doCmd('nextSimilarItem'))
      .call(doCmd('previousSimilarItem'))
      .expectSpeech('inner', 'Link')
      .call(doCmd('nextHeading'))
      .expectSpeech('outer2', 'Heading 3')
      .call(doCmd('previousSimilarItem'))
      .expectSpeech('inner', 'Heading 3');

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'InvalidItemNavigation', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h3><a href="#a">inner</a></h3>
    <p>some <span aria-invalid="spelling">txet</span></p>
    <button>button A</button>
    <p aria-invalid="true">some other reason</p>
    <p>no error text 1</P>
    <p aria-invalid=false>no error text 2</P>
    <p><span aria-invalid="grammar">this are</span> a test</span></p>
    <p aria-invalid="unknown">error is this</p>
    <a href="#b">outer1</a>
    <h3>outer2</h3>
  `;

  const root = await this.runWithLoadedTree(site);
  assertEquals(RoleType.LINK, ChromeVoxRange.current.start.node.role);
  assertEquals('inner', ChromeVoxRange.current.start.node.name);
  mockFeedback.call(doCmd('nextInvalidItem'))
      .expectSpeech('txet', 'misspelled')
      .call(doCmd('nextInvalidItem'))
      .expectSpeech('some other reason')
      .call(doCmd('nextInvalidItem'))
      .expectSpeech('this are', 'grammar error')
      .call(doCmd('nextInvalidItem'))
      .expectSpeech('error is this')
      // Ensure wrap.
      .call(doCmd('nextInvalidItem'))
      .expectSpeech('txet')
      // Wrap backward.
      .call(doCmd('previousInvalidItem'))
      .expectSpeech('error is this')
      .call(doCmd('previousInvalidItem'))
      .expectSpeech('this are', 'grammar error');

  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'InvalidItemNavigationNoItem', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <h3><a href="#a">inner</a></h3>
    <p>some text</p>
    <button>some other text</button>
    <a href="#b">outer1</a>
    <h3>outer2</h3>
  `;
      const root = await this.runWithLoadedTree(site);
      assertEquals(RoleType.LINK, ChromeVoxRange.current.start.node.role);
      assertEquals('inner', ChromeVoxRange.current.start.node.name);
      mockFeedback.call(doCmd('nextInvalidItem'))
          .expectSpeech('No invalid item')
          .call(doCmd('previousInvalidItem'))
          .expectSpeech('No invalid item');

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'TableWithAriaRowCol', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="table">
      <div role="row" aria-rowindex=3>
        <div role="cell">test</div>
      </div>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('fullyDescribe'))
      .expectSpeech('test', 'row 3 column 1', 'Table , 1 by 1');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NonModalDialogHeadingJump', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <h2>Heading outside dialog</h2>
    <div role="dialog">
      <h2>Heading inside dialog</h2>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.call(doCmd('nextHeading'))
          .expectSpeech('Heading inside dialog')
          .call(doCmd('previousHeading'))
          .expectSpeech('Heading outside dialog');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'LevelEndsForNestedLists', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div>
      <ul>
        <li>Berries
          <ul>
            <li>Strawberries</li>
            <li>Blueberries</li>
            <li>Raspberries</li>
          </ul>
        </li>
        <li>Citruses
          <ul>
              <li>Oranges
                <ul>
                  <li>Grapefruits</li>
                  <li>Mandarins</li>
                </ul>
              </li>
          </ul>
        </li>
        <li>Bananas</li>
      </ul>
    </div>
  `;

      const root = await this.runWithLoadedTree(site);
      const blueberries = root.find({attributes: {name: 'Blueberries'}});
      const grapefruits = root.find({attributes: {name: 'Grapefruits'}});

      mockFeedback
          .call(() => ChromeVoxRange.set(CursorRange.fromNode(blueberries)))
          .call(doCmd('nextObject'))
          .expectSpeech(
              '◦ Raspberries', 'List item', 'List end', 'nested level 2')
          .call(() => ChromeVoxRange.set(CursorRange.fromNode(grapefruits)))
          .call(doCmd('nextObject'))
          .expectSpeech(
              '■ Mandarins', 'List item', 'List end', 'nested level 3')
          .call(doCmd('nextObject'))
          // Nested level is not mentioned for level 1.
          .expectSpeech('• Bananas', 'List item', 'List end');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NestedListNavigationSimple', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.nestedListDoc);
      mockFeedback.expectSpeech('• Lemons', 'List item', 'List', 'with 4 items')
          .call(doCmd('nextObject'))
          .expectSpeech('• Oranges', 'List item')
          .call(doCmd('nextObject'))
          .expectSpeech('• ', 'Berries', 'List item')
          .expectBraille('• Berries lstitm')
          .call(doCmd('nextObject'))
          .expectSpeech('◦ Strawberries', 'List item', 'List', 'with 2 items')
          .call(doCmd('nextObject'))
          .expectSpeech('◦ Raspberries', 'List item', 'List end')
          .call(doCmd('nextObject'))
          .expectSpeech('• Bananas', 'List item', 'List end')
          .expectBraille('• Bananas lstitm lst end');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NestedListNavigationMixed', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.nestedListDoc);
      mockFeedback.expectSpeech('• Lemons', 'List item', 'List', 'with 4 items')
          .call(doCmd('nextObject'))
          .expectSpeech('• Oranges', 'List item')
          .call(doCmd('nextLine'))
          .expectSpeech('• ', 'Berries', 'List item')
          .call(doCmd('nextLine'))
          .expectSpeech('◦ Strawberries', 'List item', 'List', 'with 2 items')
          .call(doCmd('previousLine'))
          .expectSpeech('• ', 'Berries')
          .call(doCmd('nextWord'))
          .expectSpeech('◦ Strawberries')
          .call(doCmd('nextWord'))
          .expectSpeech('◦ Raspberries')
          .call(doCmd('previousObject'))
          .call(doCmd('previousObject'))
          .expectSpeech('• ', 'Berries')
          .call(doCmd('previousCharacter'))
          .call(doCmd('previousCharacter'))
          .call(doCmd('previousCharacter'))
          .expectSpeech('g')  // For Oranges
          .call(doCmd('nextGroup'))
          .expectSpeech('◦ Strawberries', '◦ Raspberries')
          .clearPendingOutput()
          .call(doCmd('previousGroup'))
          .expectSpeech('• Oranges');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'NavigationByList', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start here</p>
    <ul>
      Drinks
      <li>Coffee</li>
      <li>Tea</li>
    </ul>
    <p>A random paragraph</p>
    <ul></ul>
    <ol>
      Lunch
      <li>Burgers</li>
      <li>Fries</li>
      <li>Soda</li>
        <ul>
          Nested list
          <li>Element</li>
        </ul>
    </ol>
    <p>Another random paragraph</p>
    <dl>
      Colors
      <dt>Red</dt>
      <dd>Description for red</dd>
      <dt>Blue</dt>
      <dd>Description for blue</dd>
      <dt>Green</dt>
      <dd>Description for green</dd>
    </dl>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('jumpToTop'))
      .call(doCmd('nextList'))
      .expectSpeech('Drinks', 'List', 'with 2 items')
      .call(doCmd('nextList'))
      .expectSpeech('List', 'with 0 items')
      .call(doCmd('nextList'))
      .expectSpeech('Lunch', 'List', 'with 3 items')
      .call(doCmd('nextList'))
      .expectSpeech('Nested list', 'List', 'with 1 item')
      .call(doCmd('nextList'))
      .expectSpeech('Colors', 'Description list', 'with 3 items')
      .call(doCmd('nextList'))
      // Ensure we wrap correctly.
      .expectSpeech('Drinks', 'List', 'with 2 items')
      .call(doCmd('nextObject'))
      .call(doCmd('nextObject'))
      .expectSpeech('\u2022 Coffee')
      // Ensure we wrap correctly and go to previous list, not top of
      // current list.
      .call(doCmd('previousList'))
      .expectSpeech('Colors')
      .call(doCmd('previousObject'))
      .expectSpeech('Another random paragraph')
      // Ensure we dive into the nested list.
      .call(doCmd('previousList'))
      .expectSpeech('Nested list', 'List', 'with 1 item')
      .call(doCmd('previousList'))
      .expectSpeech('Lunch')
      .call(doCmd('nextObject'))
      .call(doCmd('nextObject'))
      .expectSpeech('1. Burgers')
      // Ensure we go to the previous list, not the top of the current
      // list.
      .call(doCmd('previousList'))
      .expectSpeech('List', 'with 0 items')
      .call(doCmd('previousObject'))
      .expectSpeech('A random paragraph')
      .call(doCmd('previousList'))
      .expectSpeech('Drinks', 'List', 'with 2 items');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NoListTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<button>Click me</button>');
  mockFeedback.call(doCmd('nextList'))
      .expectSpeech('No next list')
      .call(doCmd('previousList'))
      .expectSpeech('No previous list');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NavigateToLastHeading', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <h1>First</h1>
    <h1>Second</h1>
    <h1>Third</h1>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('jumpToTop'))
      .expectSpeech('First', 'Heading 1')
      .call(doCmd('previousHeading'))
      .expectSpeech('Third', 'Heading 1');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ReadLinkURLTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <a href="https://www.google.com/">A popular link</a>
    <button>Not a link</button>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextLink'))
      .expectSpeech('A popular link', 'Link', 'Press Search+Space to activate')
      .call(doCmd('readLinkURL'))
      .expectSpeech('Link URL: https://www.google.com/')
      .call(doCmd('nextObject'))
      .expectSpeech('Not a link', 'Button', 'Press Search+Space to activate')
      .call(doCmd('readLinkURL'))
      .expectSpeech('No URL found');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NoRepeatTitle', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="button" aria-label="title" title="title"></div>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('title')
      .expectSpeech('Button')
      .expectNextSpeechUtteranceIsNot('title')
      .expectSpeech('Press Search+Space to activate');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'PhoneticsAndCommands', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>some sample text</p>
    <button>ok</button>
    <p>A</p>
  `;
  const root = await this.runWithLoadedTree(site);
  const noPhonetics = {phoneticCharacters: undefined};
  const phonetics = {phoneticCharacters: true};
  mockFeedback.call(doCmd('nextObject'))
      .expectSpeechWithProperties(noPhonetics, 'ok')
      .call(doCmd('previousObject'))
      .expectSpeechWithProperties(noPhonetics, 'some sample text')
      .call(doCmd('nextWord'))
      .expectSpeechWithProperties(noPhonetics, 'sample')
      .call(doCmd('previousWord'))
      .expectSpeechWithProperties(noPhonetics, 'some')
      .call(doCmd('nextCharacter'))
      .expectSpeechWithProperties(phonetics, 'o')
      .call(doCmd('nextCharacter'))
      .expectSpeechWithProperties(phonetics, 'm')
      .call(doCmd('previousCharacter'))
      .expectSpeechWithProperties(phonetics, 'o')
      .call(doCmd('jumpToBottom'))
      .expectSpeechWithProperties(noPhonetics, 'A');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'ToggleScreen', async function() {
  const mockFeedback = this.createMockFeedback();
  // Pretend we've already accepted the confirmation dialog once.
  LocalStorage.set('acceptToggleScreen', true);
  await this.runWithLoadedTree('<div>Unimportant web content</div>');
  mockFeedback.call(doCmd('toggleScreen'))
      .expectSpeech('Screen off')
      .call(doCmd('toggleScreen'))
      .expectSpeech('Screen on')
      .call(doCmd('toggleScreen'))
      .expectSpeech('Screen off');
  await mockFeedback.replay();
});

// Tests the behavior of ChromeVox when Talkback is disabled and there is no
// current focus. We set no focus by modifying the internal ChromeVox state.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NoFocusTalkBackDisabledInternalState',
    async function() {
      // Fire onCustomSpokenFeedbackEnabled event to communicate that Talkback
      // is off for the current app.
      this.dispatchOnCustomSpokenFeedbackToggledEvent(false);
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree('<p>Test document</p>');
      ChromeVoxRange.set(null);
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech(
              'No current ChromeVox focus. Press Alt+Shift+L to go to the ' +
              'launcher.')
          .call(doCmd('previousObject'))
          .expectSpeech(
              'No current ChromeVox focus. Press Alt+Shift+L to go to the ' +
              'launcher.');
      await mockFeedback.replay();
    });

// Tests the behavior of ChromeVox when Talkback is disabled and there is no
// current focus. We set no focus by modifying the automation API.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NoFocusTalkBackDisabledAutomation',
    async function() {
      this.dispatchOnCustomSpokenFeedbackToggledEvent(false);
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree('<p>Test document</p>');
      chrome.automation.getFocus = (callback) => callback(null);
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech(
              'No current ChromeVox focus. Press Alt+Shift+L to go to the ' +
              'launcher.')
          .call(doCmd('previousObject'))
          .expectSpeech(
              'No current ChromeVox focus. Press Alt+Shift+L to go to the ' +
              'launcher.');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NoFocusTalkBackEnabled', async function() {
      // Fire onCustomSpokenFeedbackEnabled event to communicate that Talkback
      // is on for the current app. We don't want to announce the no-focus hint
      // message when TalkBack is on because we expect ChromeVox to have no
      // focus in that case. If we announce the hint message, TalkBack and
      // ChromeVox will try to speak at the same time.
      this.dispatchOnCustomSpokenFeedbackToggledEvent(true);
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree('<p>Start here</p>');
      ChromeVoxRange.set(null);
      mockFeedback.call(doCmd('nextObject'))
          .call(
              () => assertFalse(mockFeedback.utteranceInQueue(
                  'No current ChromeVox focus. ' +
                  'Press Alt+Shift+L to go to the launcher.')))
          .call(doCmd('previousObject'))
          .call(
              () => assertFalse(mockFeedback.utteranceInQueue(
                  'No current ChromeVox focus. ' +
                  'Press Alt+Shift+L to go to the launcher.')));
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'NavigateOutOfMultiline', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>start</p>
    <p>before</p>
    <div contenteditable>
      Testing testing<br>one two three
    </div>
    <p>after</p>
  `;
      const root = await this.runWithLoadedTree(site);
      const contentEditable =
          root.find({attributes: {nonAtomicTextFieldRoot: true}});
      mockFeedback.call(focus(contentEditable))
          .expectSpeech(/Testing testing\s+one two three/)
          .call(doCmd('nextLine'))
          .expectSpeech('one two three')
          .call(doCmd('nextLine'))
          .expectSpeech('after')

          // In reverse (explicitly focus, instead of moving to previous
          // line, because all subsequent commands require the content
          // editable to be focused first):
          .clearPendingOutput()
          .call(focus(contentEditable))
          .expectSpeech(/Testing testing\s+one two three/)
          .call(doCmd('nextLine'))
          .expectSpeech('one two three')
          .call(doCmd('previousLine'))
          .expectSpeech('Testing testing')
          .call(doCmd('previousLine'))
          .expectSpeech('before');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'ReadWindowTitle', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <button id="click"></button>
    <script>
      const button = document.getElementById('click');
      button.addEventListener('click', () => document.title = 'bar');
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  const clickButtonThenReadCurrentTitle = () => {
    const desktop = root.parent.root;
    desktop.addEventListener(EventType.TREE_CHANGED, evt => {
      if (evt.target.role === RoleType.WINDOW && /bar/.test(evt.target.name)) {
        doCmd('readCurrentTitle')();
      }
    });
    const button = root.find({role: RoleType.BUTTON});
    button.doDefault();
  };

  mockFeedback.clearPendingOutput()
      .call(clickButtonThenReadCurrentTitle)

      // This test may run against official builds, so match against
      // utterances starting with 'bar'. This should exclude any other
      // utterances that contain 'bar' e.g. data:...bar.. or the data url.
      .expectSpeech(/^bar*/);
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'OutputEmptyQueueMode', async function() {
  class FakeOutputAction extends OutputAction {
    run() {}
  }

  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<p>unused</p>');
  const output = new Output();
  Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);
  output.append(
      output.speechBuffer_, new Spannable(''),
      {annotation: [new FakeOutputAction()]});
  output.withString('test');
  mockFeedback.clearPendingOutput()
      .call(output.go.bind(output))
      .expectSpeechWithQueueMode('', QueueMode.CATEGORY_FLUSH)
      .expectSpeechWithQueueMode('test', QueueMode.CATEGORY_FLUSH);
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'SetAccessibilityFocus', async function() {
  const root =
      await this.runWithLoadedTree('<p>Text.</p><button>Button</button>');
  const node = root.find({role: RoleType.BUTTON});

  node.addEventListener(EventType.FOCUS, this.newCallback(function() {
    chrome.automation.getAccessibilityFocus(focusedNode => {
      assertEquals(node, focusedNode);
    });
  }));

  node.focus();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'MenuItemRadio', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <ul role="menu" tabindex="0" autofocus>
      <li role="menuitemradio" aria-checked="true">Small</li>
      <li role="menuitemradio" aria-checked="false">Medium</li>
      <li role="menuitemradio" aria-checked="false">Large</li>
    </ul>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('Menu', 'with 3 items')
      .call(doCmd('nextObject'))
      .expectSpeech('Small, menu item radio button selected', ' 1 of 3 ')
      .call(doCmd('nextObject'))
      .expectSpeech('Medium, menu item radio button unselected', ' 2 of 3 ');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ButtonNavigationIgnoresRadioButtons',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
        <button>Action 1</button>
        <fieldset>
          <p><label> <input type=radio>Radio 1</label></p>
          <p><label> <input type=radio>Radio 2</label></p>
        </fieldset>
        <button>Action 2</button>
      `;

      const root = await this.runWithLoadedTree(site);
      mockFeedback.call(doCmd('nextButton'))
          .expectSpeech('Action 1', 'Button')
          .call(doCmd('nextButton'))
          .expectSpeech('Action 2', 'Button');

      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'FocusableNamedDivIsNotContainer',
    async function() {
      const site = `
        <div aria-label="hello world" tabindex="0">hello world</div>
      `;
      const root = await this.runWithLoadedTree(site);
      const genericContainer = root.find({role: RoleType.GENERIC_CONTAINER});
      assertTrue(AutomationPredicate.object(genericContainer));
      assertFalse(AutomationPredicate.container(genericContainer));
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'HitTestOnExoSurface', async function() {
  const site = `
    <button></button>
    <input type="text"</input>
  `;
  const root = await this.runWithLoadedTree(site);
  const fakeWindow = root.find({role: RoleType.BUTTON});
  const realTextField = root.find({role: RoleType.TEXT_FIELD});

  // Fake the role and className to imitate a ExoSurface.
  Object.defineProperty(fakeWindow, 'role', {get: () => RoleType.WINDOW});
  Object.defineProperty(fakeWindow, 'className', {get: () => 'ExoSurface-40'});

  // Mock and expect a call for the fake window.
  chrome.accessibilityPrivate.sendSyntheticMouseEvent =
      this.newCallback(evt => {
        assertEquals(fakeWindow.location.left, evt.x);
        assertEquals(fakeWindow.location.top, evt.y);
      });

  // Fake a mouse explore event on the real text field. This should not
  // trigger the above mouse path.
  GestureCommandHandler.instance.pointerHandler_.onMouseMove(
      realTextField.location.left, realTextField.location.top);

  // Fake a touch explore gesture event on the fake window which should
  // trigger a mouse move.
  GestureCommandHandler.instance.onAccessibilityGesture_(
      Gesture.TOUCH_EXPLORE, fakeWindow.location.left, fakeWindow.location.top);
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'PointerSkipsContainers', async function() {
      PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS = -1;
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div role="grouparia-label="test" " tabindex=0>
      <div role=button><p></p></div>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      ChromeVoxRange.addObserver(new class {
        onCurrentRangeChanged(range) {
          if (!range) {
            ChromeVox.tts.speak('range cleared!');
          }
        }
      }());

      const button = root.find({role: RoleType.BUTTON});
      assertNotNullNorUndefined(button);
      const group = button.parent;
      assertNotNullNorUndefined(group);
      mockFeedback.call(simulateHitTestResult(button))
          .expectSpeech('Button')
          .call(() => {
            // Override the role to simulate panes which are only found in
            // views.
            Object.defineProperty(group, 'role', {
              get() {
                return chrome.automation.RoleType.PANE;
              },
            });
          })
          .call(simulateHitTestResult(group))
          .expectSpeech('range cleared!')
          .expectEarcon(EarconId.NO_POINTER_ANCHOR)
          .call(simulateHitTestResult(button))
          .expectSpeech('Button')
          .call(simulateHitTestResult(group))
          .expectSpeech('range cleared!')
          .expectEarcon(EarconId.NO_POINTER_ANCHOR);
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'FocusOnUnknown', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role="group" tabindex=0>
      <p>hello<p>
    </div>
    <div role="group" tabindex=0></div>
  `;
  const root = await this.runWithLoadedTree(site);
  const [group1, group2] = root.findAll({role: RoleType.GROUP});
  assertNotNullNorUndefined(group1);
  assertNotNullNorUndefined(group2);
  Object.defineProperty(group1, 'role', {
    get() {
      return chrome.automation.RoleType.UNKNOWN;
    },
  });
  Object.defineProperty(group2, 'role', {
    get() {
      return chrome.automation.RoleType.UNKNOWN;
    },
  });

  const evt2 = new CustomAutomationEvent(EventType.FOCUS, group2);
  const currentRange = ChromeVoxRange.current;
  DesktopAutomationInterface.instance.onFocus_(evt2);
  assertEquals(currentRange, ChromeVoxRange.current);

  const evt1 = new CustomAutomationEvent(EventType.FOCUS, group1);
  mockFeedback
      .call(DesktopAutomationInterface.instance.onFocus_.bind(
          DesktopAutomationInterface.instance, evt1))
      .expectSpeech('hello');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'TimeDateCommand', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<p></p>');
  mockFeedback.call(doCmd('speakTimeAndDate'))
      .expectSpeech(/(AM|PM)*(2)/)
      .expectBraille(/(AM|PM)*(2)/);
  await mockFeedback.replay();
});

// TODO(https://crbug.com/1395217): Re-enable the test.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'DISABLED_SwipeToScrollByPage',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
    <p style="font-size: 200pt">This is a test</p>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.call(doGesture(Gesture.SWIPE_UP3))
          .expectSpeech(/Page 2 of/)
          .call(doGesture(Gesture.SWIPE_UP3))
          .expectSpeech(/Page 3 of/)
          .call(doGesture(Gesture.SWIPE_DOWN3))
          .expectSpeech(/Page 2 of/)
          .call(doGesture(Gesture.SWIPE_DOWN3))
          .expectSpeech(/Page 1 of/);
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'PointerOnOffOnRepeatsNode', async function() {
      PointerHandler.MIN_NO_POINTER_ANCHOR_SOUND_DELAY_MS = -1;
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree('<button>hi</button>');
      ChromeVoxRange.addObserver(new class {
        onCurrentRangeChanged(range) {
          if (!range) {
            ChromeVox.tts.speak('range cleared!');
          }
        }
      }());

      const button = root.find({role: RoleType.BUTTON});
      assertNotNullNorUndefined(button);
      mockFeedback.call(simulateHitTestResult(button))
          .expectSpeech('hi', 'Button')

          // Touch slightly off of the button.
          .call(GestureCommandHandler.instance.onAccessibilityGesture_.bind(
              GestureCommandHandler.instance, Gesture.TOUCH_EXPLORE,
              button.location.left, button.location.top + 60))
          .expectSpeech('range cleared!')
          .expectEarcon(EarconId.NO_POINTER_ANCHOR)
          .clearPendingOutput()
          .call(simulateHitTestResult(button))
          .expectSpeech('hi', 'Button');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'SelectCollapsed', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <select id="button">
      <option value="Apple">Apple</option>
      <option value="Banana">Banana</option>
    </select>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('jumpToTop'))
      .expectSpeech(
          'Apple', 'Button', 'has pop up', 'Collapsed',
          'Press Search+Space to activate');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'PopupButtonExpanded', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <button id="button" aria-haspopup="true" aria-expanded="true"
        aria-controls="menu">
      Click me
    </button>
    <ul id="menu"
      role="menu"
      aria-labelledby="button">
      <li role="menuitem">Item 1</li>
      <li role="menuitem">Item 2</li>
      <li role="menuitem">Item 3</li>
    </ul>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback
      .call(doCmd('jumpToTop'))
      // SetSize is only reported if popup button is expanded.
      .expectSpeech(
          'Click me', 'Button', 'has pop up', 'with 3 items', 'Expanded',
          'Press Search+Space to activate');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'SortDirection', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <table border="1">
      <th aria-sort="ascending"><button id="sort">Date</button></th>
      <tr><td>1/2/20</td></tr>
      <tr><td>2/2/20</td></tr>
    </table>
    <script>
      let ascending = true;
      const sortButton = document.getElementById('sort');
      sortButton.addEventListener('click', () => {
        ascending = !ascending;
        sortButton.parentElement.setAttribute(
            'aria-sort', ascending ? 'ascending' : 'descending');
      });
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  const sortButton = root.find({role: RoleType.BUTTON});
  mockFeedback.expectSpeech('Button', 'Ascending sort')
      .call(doDefault(sortButton))
      .expectSpeech('Descending sort')
      .call(doDefault(sortButton))
      .expectSpeech('Ascending sort')
      .call(doDefault(sortButton))
      .expectSpeech('Descending sort');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'InlineLineNavigation', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <p><strong>This</strong><b>is</b>a <em>test</em></p>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextLine')).expectSpeech('This', 'is', 'a ', 'test');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'AudioVideo', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <button></button>
    <button></button>
  `;
  const root = await this.runWithLoadedTree(site);
  const [audio, video] = root.findAll({role: RoleType.BUTTON});

  assertNotNullNorUndefined(audio);
  assertNotNullNorUndefined(video);

  assertEquals(undefined, audio.name);
  assertEquals(undefined, video.name);
  assertEquals(undefined, audio.firstChild);
  assertEquals(undefined, video.firstChild);

  // Fake the roles.
  Object.defineProperty(audio, 'role', {
    get() {
      return chrome.automation.RoleType.AUDIO;
    },
  });

  Object.defineProperty(video, 'role', {
    get() {
      return chrome.automation.RoleType.VIDEO;
    },
  });

  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('Video')
      .call(doCmd('previousObject'))
      .expectSpeech('Audio');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'AlertNoAnnouncement', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<button></button>');
  ChromeVoxRange.addObserver(new class {
    onCurrentRangeChanged(range) {
      assertNotReached('Range was changed unexpectedly.');
    }
  }());
  const button = root.find({role: RoleType.BUTTON});
  const alertEvt = new CustomAutomationEvent(EventType.ALERT, button);
  mockFeedback
      .call(DesktopAutomationInterface.instance.onAlert_.bind(
          DesktopAutomationInterface.instance, alertEvt))
      .call(() => assertFalse(mockFeedback.utteranceInQueue('Alert')));
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'AlertAnnouncement', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<button>hello world</button>');
  ChromeVoxRange.addObserver(new class {
    onCurrentRangeChanged(range) {
      assertNotReached('Range was changed unexpectedly.');
    }
  }());

  const button = root.find({role: RoleType.BUTTON});
  const alertEvt = new CustomAutomationEvent(EventType.ALERT, button);
  mockFeedback
      .call(DesktopAutomationInterface.instance.onAlert_.bind(
          DesktopAutomationInterface.instance, alertEvt))
      .expectNextSpeechUtteranceIsNot('Alert')
      .expectSpeech('hello world');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'SwipeLeftRight4ByContainers', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`<p>test</p>`);
      mockFeedback.call(doGesture(Gesture.SWIPE_RIGHT4))
          .expectSpeech('Launcher', 'Button', 'Shelf', 'Tool bar', ', window')
          .call(doGesture(Gesture.SWIPE_RIGHT4))
          .expectSpeech('Shelf', 'Tool bar')
          .call(doGesture(Gesture.SWIPE_RIGHT4))
          .expectSpeech(/Calendar*/)
          .call(doGesture(Gesture.SWIPE_RIGHT4))
          .expectSpeech(/Address and search bar*/)

          .call(doGesture(Gesture.SWIPE_LEFT4))
          .expectSpeech(/Calendar*/)
          .call(doGesture(Gesture.SWIPE_LEFT4))
          .expectSpeech('Shelf', 'Tool bar');

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'SwipeLeftRight2', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p id="live" aria-live="polite"</p>
    <script>
    document.body.addEventListener('keydown', evt => {
      document.getElementById('live').textContent = evt.key;
    });
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doGesture(Gesture.SWIPE_RIGHT2)).expectSpeech('Enter');
  mockFeedback.call(doGesture(Gesture.SWIPE_LEFT2)).expectSpeech('Escape');
  await mockFeedback.replay();
});

// TODO(crbug.com/40777708) - Improve the generation of summaries across ChromeOS
AX_TEST_F(
    // TODO(crbug.com/1419811): Test is flaky.
    'ChromeVoxBackgroundTest', 'DISABLED_AlertDialogAutoSummaryTextContent',
    async function() {
      this.resetContextualOutput();
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>start</p>
    <div role="alertdialog" aria-label="Setup">
      <h1>Welcome</h1>
      <p>This is some introductory text<p>
      <button>Exit</button>
      <button>Let's go</button>
    </div>
    <p>end</p>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('Setup')
          .expectSpeech(`Welcome This is some introductory text Exit Let's go`)
          .expectSpeech('Welcome', 'Heading 1')
          .call(doCmd('nextObject'))
          .expectSpeech('This is some introductory text')
          .call(doCmd('nextObject'))
          .expectSpeech('Exit', 'Button')
          .call(doCmd('nextObject'))
          .expectSpeech(`Let's go`, 'Button')
          .call(doCmd('nextObject'))
          .expectSpeech('end')

          .call(doCmd('previousObject'))
          .expectSpeech(`Let's go`, 'Button')
          .expectSpeech('Setup')
          .expectSpeech(`Welcome This is some introductory text Exit Let's go`);

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'ImageAnnotations', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <img alt="bar" src="data:image/png;base64,iVBORw0KGgoAAAANS">
    <img src="data:image/png;base64,iVBORw0KGgoAAAANS">
  `;
  const root = await this.runWithLoadedTree(site);
  const [namedImg, unnamedImg] = root.findAll({role: RoleType.IMAGE});

  assertNotNullNorUndefined(namedImg);
  assertNotNullNorUndefined(unnamedImg);

  assertEquals('bar', namedImg.name);
  assertEquals(undefined, unnamedImg.name);

  // Fake the image annotation.
  Object.defineProperty(namedImg, 'imageAnnotation', {
    get() {
      return 'foo';
    },
  });
  Object.defineProperty(unnamedImg, 'imageAnnotation', {
    get() {
      return 'foo';
    },
  });

  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('start')
      .expectNextSpeechUtteranceIsNot('foo')
      .expectSpeech('bar', 'Image')
      .call(doCmd('nextObject'))
      .expectNextSpeechUtteranceIsNot('bar')
      .expectSpeech('foo', 'Image');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'VolumeChanges', async function() {
  const mockFeedback = this.createMockFeedback();
  await this.runWithLoadedTree('<p>test</p>');
  const bounds = FocusBounds.get();
  mockFeedback.call(press(KeyCode.VOLUME_UP))
      .expectSpeech('Volume', 'Slider', /\d+%/)
      .call(() => {
        // The bounds should not have changed.
        assertEquals(JSON.stringify(bounds), JSON.stringify(FocusBounds.get()));
      });
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'WrapContentEditableAtEndOfDoc',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<p>start</p>
      <div role="textbox" contenteditable aria-multiline="false"></div>`;
      await this.runWithLoadedTree(site);
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('Edit text')
          .call(doCmd('nextObject'))
          .expectEarcon(EarconId.WRAP)
          .expectSpeech('Web Content')
          .call(doCmd('nextObject'))
          .expectSpeech('start');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ReadFromHereBlankNodes', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<a tabindex=0></a><p>start</p><a tabindex=0></a><p>end</p>`;
      const root = await this.runWithLoadedTree(site);
      assertEquals(
          RoleType.STATIC_TEXT, ChromeVoxRange.current.start.node.role);

      // "start" is uttered twice, once for the initial focus as the page loads,
      // and once during the 'read from here' command.
      mockFeedback.expectSpeech('start')
          .call(doCmd('readFromHere'))
          .expectSpeech('start', 'end');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'ContainerButtons', async function() {
  const mockFeedback = this.createMockFeedback();

  // This pattern can be found in ARC++/YouTube.
  const site = `
    <p>videos</p>
    <div aria-label="Cat Video" role="button">
      <div role="group">4 minutes, Cat Video</div>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  const group = root.find({role: RoleType.GROUP});

  Object.defineProperty(group, 'clickable', {
    get() {
      return true;
    },
  });

  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('Cat Video', 'Button')
      .call(doCmd('nextObject'))
      .expectSpeech('4 minutes, Cat Video');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'FocusOnWebAreaIgnoresEvents', async function() {
      const site = `
    <div role="application" tabindex=0 aria-label="container">
      <select>
        <option>apple</option>
        <option>grape</option>
        <option>pear</option>
      </select>
    </div>
    <p>go</p>
    <script>
      let counter = 0;
      document.body.getElementsByTagName('p')[0].addEventListener('click',
          e => {
            document.body.getElementsByTagName('select')[0].selectedIndex =
                ++counter % 3;
          });
    </script>
  `;
      const root = await this.runWithLoadedTree(site);
      const application = root.find({role: RoleType.APPLICATION});
      const popUpButton = root.find({role: RoleType.COMBO_BOX_SELECT});
      const p = root.find({role: RoleType.PARAGRAPH});

      // Move focus to the select, which honors value changes through
      // FocusAutomationHandler.
      popUpButton.focus();
      await TestUtils.waitForSpeech('apple');

      // Clicking the paragraph programmatically changes the select value.
      p.doDefault();
      await TestUtils.waitForSpeech('grape');
      assertEquals(
          RoleType.COMBO_BOX_SELECT, ChromeVoxRange.current.start.node.role);

      // Now, move focus to the application which is a parent of the select.
      application.focus();
      await TestUtils.waitForSpeech('container');

      // Hook into the speak call, to see what comes next.
      let nextSpeech;
      ChromeVox.tts.speak = textString => {
        nextSpeech = textString;
      };

      // Trigger another value update for the select.
      p.doDefault();

      // This comes when the <select>'s value changes.
      await this.waitForEvent(application, EventType.SELECTED_VALUE_CHANGED);

      // Nothing should have been spoken.
      assertEquals(undefined, nextSpeech);
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'AriaLeaves', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="radio"><p>PM</p></div>
    <div role="switch"><p>Agree</p></div>
    <div role="checkbox"><p>Agree</p></div>
    <script>
      const p = document.getElementsByTagName('p')[0];
      p.addEventListener('click', () => {});
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('PM, radio button unselected')
      .call(doCmd('nextObject'))
      .expectSpeech('PM')
      .call(
          () => assertEquals(
              RoleType.STATIC_TEXT, ChromeVoxRange.current.start.node.role))

      .call(doCmd('nextObject'))
      .expectSpeech('Agree, switch off')
      .call(
          () => assertEquals(
              RoleType.SWITCH, ChromeVoxRange.current.start.node.role))

      .call(doCmd('nextObject'))
      .expectSpeech('Agree', 'Check box')
      .call(
          () => assertEquals(
              RoleType.CHECK_BOX, ChromeVoxRange.current.start.node.role));

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'MarkedContent', async function() {
  this.resetContextualOutput();
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <span>This is </span><span role="mark">my</span><span> text.</span>
    <br>
    <span>This is </span><span role="mark"
        aria-roledescription="Comment">your</span><span> text.</span>
    <br>
    <span>This is </span><span role="suggestion"><span
        role="insertion">their</span></span><span> text.</span>
    <br>
    <span>This is </span><span role="suggestion"><span
        role="deletion">everyone's</span></span><span> text.</span>
  `;
  const rootNode = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('Start')
      .call(doCmd('nextObject'))
      .expectSpeech('This is ')
      .call(doCmd('nextObject'))
      .expectSpeech('Marked content', 'my', 'Marked content end')
      .expectBraille('Marked content my Marked content end')
      .call(doCmd('nextObject'))
      .expectSpeech(' text.')
      .expectBraille(' text.')
      .call(doCmd('nextObject'))
      .expectSpeech('This is ')
      .call(doCmd('nextObject'))
      .expectSpeech('Comment', 'your', 'Comment end')
      .expectBraille('Comment your Comment end')
      .call(doCmd('nextObject'))
      .expectSpeech(' text.')
      .expectBraille(' text.')
      .call(doCmd('nextObject'))
      .expectSpeech('This is ')
      .call(doCmd('nextObject'))
      .expectSpeech('Suggest', 'Insert', 'their', 'Insert end', 'Suggest end')
      .expectBraille('Suggest Insert their Insert end Suggest end')
      .call(doCmd('nextObject'))
      .expectSpeech(' text.')
      .expectBraille(' text.')
      .call(doCmd('nextObject'))
      .expectSpeech('This is ')
      .call(doCmd('nextObject'))
      .expectSpeech(
          'Suggest', 'Delete', `everyone's`, 'Delete end', 'Suggest end')
      .expectBraille(`Suggest Delete everyone's Delete end Suggest end`)
      .call(doCmd('nextObject'))
      .expectSpeech(' text.')
      .expectBraille(' text.');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ClickAncestorAreNotActionable',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>Start</p>
    <div id="button1" role="button" aria-label="OK">
      <div role="group">OK</div>
    </div>
    <div id="button2" role="button" aria-label="cancel">
      <a href="#cancel">more info</a>
    </div>
    <p>end</p>
    <script>
      document.getElementById('button1').addEventListener('click', () => {});
      document.getElementById('button2').addEventListener('click', () => {});
    </script>
  `;
      const rootNode = await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('Start')
          .call(doCmd('nextObject'))
          .expectSpeech('OK')
          .call(doCmd('nextObject'))
          .expectSpeech('cancel')
          .call(doCmd('nextObject'))
          .expectSpeech('more info')
          .call(doCmd('nextObject'))
          .expectSpeech('end');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'TouchEditingState', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <input type="text"></input>
  `;
  const rootNode = await this.runWithLoadedTree(site);
  const bounds = rootNode.find({role: RoleType.TEXT_FIELD}).location;
  mockFeedback.expectSpeech('Start')
      .call(doGesture(
          chrome.accessibilityPrivate.Gesture.TOUCH_EXPLORE, bounds.left,
          bounds.top))
      .expectSpeech('Edit text', 'Double tap to start editing')
      .call(doGesture(
          chrome.accessibilityPrivate.Gesture.CLICK, bounds.left, bounds.top))
      .expectSpeech('Edit text', 'is editing');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'TouchGesturesProducesEarcons',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>Start</p>
    <button>ok</button>
    <a href="chromevox.com">cancel</a>
  `;
      const rootNode = await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('Start')
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
          .expectSpeech('ok', 'Button')
          .expectEarcon(EarconId.BUTTON)
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
          .expectSpeech('cancel', 'Link')
          .expectEarcon(EarconId.LINK)
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_LEFT1))
          .expectSpeech('ok', 'Button')
          .expectEarcon(EarconId.BUTTON);
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'Separator', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <p><span>Hello</span></p>
    <p><span role="separator">Separator content should be read</span></p>
    <p><span>World</span></p>
  `;
  const rootNode = await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('Start')
      .call(doCmd('nextObject'))
      .expectSpeech('Hello')
      .call(doCmd('nextObject'))
      .expectSpeech('Separator content should be read', 'Separator')
      .expectBraille('Separator content should be read seprtr')
      .call(doCmd('nextObject'))
      .expectSpeech('World');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'FocusAfterClick', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>Start</p>
    <button id="clickMe">Click me</button>
    <h1 id="focusMe" tabindex=-1>Focus me</h1>
    <script>
      document.getElementById('clickMe').addEventListener('click', function() {
        document.getElementById('focusMe').focus();
      }, false);
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  BaseAutomationHandler.announceActions = false;
  mockFeedback.expectSpeech('Start')
      .call(doCmd('nextObject'))
      .expectSpeech('Click me')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech('Focus me')
      .call(
          () =>
              assertEquals('Focus me', ChromeVoxRange.current.start.node.name));
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'EarconPlayback', function() {
  const engine = ChromeVox.earcons.engine_;
  assertTrue(engine !== undefined);

  // We only test a few earcons here. Not all earcons prevent parallel playback
  // or have mappings into the earcon engine.

  // Ensure there are no tracked sources yet.
  engine.lastEarconSources_ = {};

  // Note that alert modal vs nonmodal would be allowed to play in parallel (as
  // do wrap / wrap edge) because they are different events even though they
  // really play the same sound.
  ChromeVox.earcons.playEarcon(EarconId.ALERT_MODAL);
  assertEquals(1, Object.keys(engine.lastEarconSources_).length);
  const lastAlertSource = engine.lastEarconSources_[EarconId.ALERT_MODAL];
  assertTrue(lastAlertSource !== undefined);

  ChromeVox.earcons.playEarcon(EarconId.ALERT_MODAL);
  assertEquals(1, Object.keys(engine.lastEarconSources_).length);

  // The earcon for this stayed the same (above), so there's no duplicate
  // playback of two alerts.
  assertEquals(
      lastAlertSource, engine.lastEarconSources_[EarconId.ALERT_MODAL]);

  // This simulates a parallel playback of the button earcon which is allowed.
  ChromeVox.earcons.playEarcon(EarconId.BUTTON);
  assertEquals(2, Object.keys(engine.lastEarconSources_).length);
  assertTrue(engine.lastEarconSources_[EarconId.BUTTON] !== undefined);

  // This gets called by web audio when the earcon finishes.
  lastAlertSource.onended();

  // The button earcon is still playing.
  assertEquals(1, Object.keys(engine.lastEarconSources_).length);
  assertTrue(engine.lastEarconSources_[EarconId.BUTTON] !== undefined);

  // Finish up the button earcon, too.
  engine.lastEarconSources_[EarconId.BUTTON].onended();
  assertEquals(0, Object.keys(engine.lastEarconSources_).length);
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'MixedNavWithRangeInvalidation',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>Start</p>
    <button>apple</button>
    <a href="google.com">grape</a>
    <button>banana</button>
  `;
      const root = await this.runWithLoadedTree(site);
      // Different ways to navigate to the next object.
      const keyboardHandler = BackgroundKeyboardHandler.instance;
      const nextObjectKeyboard =
          keyboardHandler.onKeyDown.bind(keyboardHandler, {
            keyCode: KeyCode.RIGHT,
            metaKey: true,
            preventDefault: () => {},
            stopPropagation: () => {},
          });
      const nextObjectBraille = BrailleCommandHandler.onBrailleKeyEvent.bind(
          BrailleCommandHandler, {command: BrailleKeyCommand.PAN_RIGHT});
      const nextObjectGesture =
          GestureCommandHandler.instance.onAccessibilityGesture_.bind(
              GestureCommandHandler.instance, Gesture.SWIPE_RIGHT1);
      const clearCurrentRange = () => ChromeVoxRange.set(null);
      const toggleTalkBack = () => {
        ChromeVoxState.instance.talkBackEnabled_ =
            !ChromeVoxState.instance.talkBackEnabled_;
      };

      mockFeedback
          .expectSpeech('Start')

          .call(clearCurrentRange)
          .call(nextObjectKeyboard)
          .expectSpeech('apple', 'Button')

          .call(clearCurrentRange)
          .call(nextObjectBraille)
          .expectSpeech('grape', 'Link')

          .call(clearCurrentRange)
          .call(nextObjectGesture)
          .expectSpeech('banana', 'Button')

          .call(clearCurrentRange)
          .call(nextObjectKeyboard)
          .expectSpeech('Web Content')

          .call(clearCurrentRange)
          .call(toggleTalkBack)
          .call(nextObjectKeyboard)
          .call(() => assertFalse(Boolean(ChromeVoxRange.current)))

          .call(nextObjectBraille)
          .call(() => assertFalse(Boolean(ChromeVoxRange.current)))

          .call(nextObjectGesture)
          .call(() => assertFalse(Boolean(ChromeVoxRange.current)))

          .call(toggleTalkBack)
          .call(nextObjectKeyboard)
          .call(() => assertTrue(Boolean(ChromeVoxRange.current)));

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'DetailsChanged', async function() {
  const mockFeedback = this.createMockFeedback();

  // Make sure we're not testing reading of the hint from the button's output
  // below.
  SettingsManager.set('useVerboseMode', false);
  const site = `
    <button id="click">ok</button>
    <p id="details">hello</p>
    <script>
      const button = document.getElementById('click');
      button.addEventListener('click', () => {
        button.setAttribute('aria-details', 'details');
      });
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  const button = root.find({role: RoleType.BUTTON});
  mockFeedback.expectSpeech('ok')
      .call(doDefault(button))
      .expectSpeech('Press Search+A, J to jump to details');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'PageLoadEarcons', function() {
  const sawEarcons = [];
  const fakeEarcons = {playEarcon: earcon => sawEarcons.push(earcon)};
  Object.defineProperty(ChromeVox, 'earcons', {get: () => fakeEarcons});
  AutomationUtil.getTopLevelRoot = node => node;

  // Use this specific object to control the load environment.
  const handler = new PageLoadSoundHandler();

  // Build up a fake automation node with a parent and root.
  const fakeNode = {};
  fakeNode.docUrl = 'foo';
  fakeNode.root = fakeNode;
  fakeNode.parent = {state: {focused: true}};

  handler.onLoadStart({target: fakeNode});
  assertEqualStringArrays([EarconId.PAGE_START_LOADING], sawEarcons);
  handler.onLoadComplete({target: fakeNode});
  assertEqualStringArrays(
      [EarconId.PAGE_START_LOADING, EarconId.PAGE_FINISH_LOADING], sawEarcons);

  // No extra earcons.
  handler.onLoadComplete({target: fakeNode});
  assertEqualStringArrays(
      [EarconId.PAGE_START_LOADING, EarconId.PAGE_FINISH_LOADING], sawEarcons);

  // Try a range change that finishes the load sound.
  sawEarcons.length = 0;
  handler.onLoadStart({target: fakeNode});
  fakeNode.docLoadingProgress = 1;
  handler.onCurrentRangeChanged({start: {node: fakeNode}});
  assertEqualStringArrays(
      [EarconId.PAGE_START_LOADING, EarconId.PAGE_FINISH_LOADING], sawEarcons);
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NewTabRead', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `<p>start</p><p>end</p>`;
  const root = await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('end')
      .call(press(KeyCode.T, {ctrl: true}))
      .expectSpeech(/New Tab/);
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NestedMenuHints', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div role="menu" aria-orientation="vertical">
      <div role="menu" aria-orientation="horizontal">
        <div tabindex="0" role="menuitem">hello</div>
        <div tabindex="0" role="menuitem">bro</div>
      </div>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  mockFeedback
      .expectSpeech('Press left or right arrow to navigate; enter to activate')
      .call(
          () => assertFalse(mockFeedback.utteranceInQueue(
              'Press up or down arrow to navigate; enter to activate')));
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'SkipLabelDescriptionFor', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>start</p>
    <label>
      <input type="checkbox" name="enableSpeechLogging">
      <span>Enable speech logging</span>
    </label>
    <p>end</p>
  `;
      const root = await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('start')
          .call(doCmd('nextObject'))
          .expectSpeech('Enable speech logging', 'Check box')
          .call(doCmd('nextObject'))
          .expectSpeech('end')
          .call(doCmd('previousObject'))
          .expectSpeech('Enable speech logging', 'Check box')
          .call(doCmd('previousObject'))
          .expectSpeech('start');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'Abbreviation', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <abbr title="uniform resource locator">URL</abbr>
  `;
  await this.runWithLoadedTree(site);
  mockFeedback.expectSpeech('URL', 'uniform resource locator', 'Abbreviation');
  await mockFeedback.replay();
});

// TODO(crbug.com/1361544): Test is flaky.
AX_TEST_F('ChromeVoxBackgroundTest', 'DISABLED_EndOfText', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div tabindex=0 role="textbox" contenteditable><p>abc</p><p>123</p></div>
  `;
  const root = await this.runWithLoadedTree(site);
  const contentEditable = root.find({role: RoleType.TEXT_FIELD});

  contentEditable.focus();
  await this.waitForEvent(contentEditable, EventType.FOCUS);
  mockFeedback.call(press(KeyCode.RIGHT))
      .expectSpeech('b')
      .call(press(KeyCode.RIGHT))
      .expectSpeech('c')
      .call(press(KeyCode.RIGHT))
      .expectSpeech('\n')
      .call(press(KeyCode.RIGHT))
      .expectSpeech('1')
      .call(press(KeyCode.RIGHT))
      .expectSpeech('2')
      .call(press(KeyCode.RIGHT))
      .expectSpeech('3')
      .call(press(KeyCode.RIGHT))
      .expectSpeech('End of text')

      .call(press(KeyCode.LEFT))
      .expectSpeech('3')
      .call(press(KeyCode.LEFT))
      .expectSpeech('2')
      .call(press(KeyCode.LEFT))
      .expectSpeech('1')
      .call(press(KeyCode.LEFT))
      .expectSpeech('\n')
      .call(press(KeyCode.LEFT))
      .expectSpeech('c')
      .call(press(KeyCode.LEFT))
      .expectSpeech('b')
      .call(press(KeyCode.LEFT))
      .expectSpeech('a');

  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ShowContextMenuOnViewsTab', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<p>test</p>`;
      const root = await this.runWithLoadedTree(site);
      const tabs = root.findAll({Role: RoleType.TAB});
      assertTrue(tabs.length > 0);
      tabs[0].showContextMenu();
      mockFeedback.expectSpeech(/menu opened/);
      await mockFeedback.replay();
    });

// TODO(crbug.com/1427939): Flaky.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'DISABLED_SelectWithOptGroup', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <select>
      <optgroup label="Theropods">
          <option>Tyrannosaurus</option>
          <option>Velociraptor</option>
          <option>Deinonychus</option>
      </optgroup>
    </select>
  `;
      await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('Tyrannosaurus', 'has pop up', 'Collapsed')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech('Tyrannosaurus')
          .call(press(KeyCode.DOWN))
          .expectSpeech('Velociraptor')
          .call(press(KeyCode.DOWN))
          .expectSpeech('Deinonychus')
          .call(press(KeyCode.UP))
          .expectSpeech('Velociraptor');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'GroupNavigation', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p><span>hello</span><a href="a.com">hi</a><a href="a.com">hey</a></p>
    <p><span>goodbye</span><a href="a.com">bye</a><a href="a.com">chow</a></p>
  `;
  await this.runWithLoadedTree(site);
  mockFeedback.call(doCmd('nextGroup'))
      .expectSpeech('goodbye', 'bye', 'Link', 'chow', 'Link')
      .call(doCmd('nextObject'))
      .expectSpeech('goodbye')
      .call(doCmd('nextObject'))
      .expectSpeech('bye', 'Link')
      .call(doCmd('previousGroup'))
      .expectSpeech('hello', 'hi', 'Link', 'hey', 'Link');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'AllowIframeToBeFocused', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>hello</p>
    <iframe id="frame" tabindex=-1
        srcdoc="<title>test title</title><p>world</p>"></iframe>
    <button id="click"></button>
    <script>
      const button = document.getElementById('click');
      button.addEventListener('click', () => {
        document.getElementById('frame').focus();
      });
    </script>
  `;
      const root = await this.runWithLoadedTree(site);
      const button = root.find({role: RoleType.BUTTON});
      mockFeedback.expectSpeech('hello')
          .call(doDefault(button))
          .expectSpeech('test title');
      await mockFeedback.replay();
    });

TEST_F('ChromeVoxBackgroundTest', 'NewWindowWebSpeech', function() {
  this.newCallback(async () => {
    const speech = [];
    let onSpeech;
    ChromeVox.tts.speak = textString => {
      speech.push(textString);
      if (onSpeech) {
        onSpeech(textString);
      }
    };

    this.runWithLoadedTree('<h1>ChromeVox Rocks</h1><p>We love ChromeVox</p>');

    await new Promise(resolve => {
      onSpeech = textString => {
        if (textString === 'ChromeVox Rocks') {
          resolve();
        }
      };
    });

    press(KeyCode.TAB)();

    await new Promise(resolve => {
      onSpeech = resolve;
    });

    // Check to ensure there are no duplicate announcements.
    assertEquals(
        speech.indexOf('ChromeVox Rocks'),
        speech.lastIndexOf('ChromeVox Rocks'));

    // Ensure there are no announcements about the Tab role.
    assertTrue(speech.every(text => {
      return text.indexOf('Tab') !== 0;
    }));
  })();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'MultipleListBoxes', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role="listbox" aria-expanded="false" aria-label="Configuration 1">
      <div role="presentation">
        <div role="presentation">
          <div aria-selected="true" role="option" tabindex="0">
            <span>Listbox item 1</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-1">
            <span>Listbox item 2</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-2">
            <span>Listbox item 3</span>
          </div>
        </div>
        <div role="presentation"></div>
      </div>
    </div>

    <div role="listbox" aria-expanded="false" aria-label="Configuration 2">
      <div role="presentation">
        <div role="presentation">
          <div aria-selected="false" role="option" tabindex="-1">
            <span>Listbox item 1</span>
          </div>
          <div aria-selected="true" role="option" tabindex="0">
            <span>Listbox item 2</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-2">
            <span>Listbox item 3</span>
          </div>
        </div>
        <div role="presentation"></div>
      </div>
    </div>

    <div role="listbox" aria-expanded="false" aria-label="Configuration 3">
      <div role="presentation">
        <div role="presentation">
          <div aria-selected="false" role="option" tabindex="-1">
            <span>Listbox item 1</span>
          </div>
          <div aria-selected="false" role="option" tabindex="-2">
            <span>Listbox item 2</span>
          </div>
          <div aria-selected="true" role="option" tabindex="0">
            <span>Listbox item 3</span>
          </div>
        </div>
        <div role="presentation"></div>
      </div>
    </div>
  `;
  await this.runWithLoadedTree(site);
  mockFeedback.call(press(KeyCode.TAB))
      .expectSpeech('Listbox item 1', ' 1 of 3 ', 'Configuration 1', 'List box')
      .call(press(KeyCode.TAB))
      .expectSpeech('Listbox item 2', ' 2 of 3 ', 'Configuration 2', 'List box')
      .call(press(KeyCode.TAB))
      .expectSpeech(
          'Listbox item 3', ' 3 of 3 ', 'Configuration 3', 'List box');
  await mockFeedback.replay();
});

// Make sure linear navigation does not go inside ListBox's options.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ListBoxLinearNavigation', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.listBoxDoc);
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('Select an item', 'List box')
          .call(doCmd('nextObject'))
          .expectSpeech('Click', 'Button')
          .call(doCmd('previousObject'))
          .expectSpeech('Select an item', 'List box');
      await mockFeedback.replay();
    });

// Make sure navigation with Tab to ListBox lands on options.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ListBoxItemsNavigation', async function() {
      const mockFeedback = this.createMockFeedback();

      await this.runWithLoadedTree(this.listBoxDoc);
      mockFeedback.call(press(KeyCode.TAB))
          .expectSpeech(
              'Listbox item one', ' 1 of 3 ', 'Select an item', 'List box')
          .call(doCmd('nextObject'))
          .expectSpeech('Listbox item two', ' 2 of 3 ')
          .call(doCmd('nextObject'))
          .expectSpeech('Listbox item three', ' 3 of 3 ');
      await mockFeedback.replay();
    });

// Make sure navigation with touch to ListBox lands on options.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'TouchListBoxItemsNavigation', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.listBoxDoc);
      mockFeedback
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
          .expectSpeech('Start')
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
          .expectSpeech(
              'Listbox item one', ' 1 of 3 ', 'Select an item', 'List box')
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
          .expectSpeech('Listbox item two', ' 2 of 3 ')
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_LEFT1))
          .expectSpeech('Listbox item one', ' 1 of 3 ')
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
          .expectSpeech('Listbox item two', ' 2 of 3 ')
          .call(doGesture(chrome.accessibilityPrivate.Gesture.SWIPE_RIGHT1))
          .expectSpeech('Listbox item three', ' 3 of 3 ');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'CrossWindowNextPreviousFocus',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
<div aria-label="first"><button>second</button><button>third</button></div>
<div aria-label="fourth"><button>fifth</button><button>sixth</button></div>
`;
      const root = await this.runWithLoadedTree(site);
      // Fake out the divs to be windows.
      const window1 = root.children[0];
      const window2 = root.children[1];
      Object.defineProperty(window1, 'role', {get: () => RoleType.WINDOW});
      Object.defineProperty(window2, 'role', {get: () => RoleType.WINDOW});

      // Linear nav should just wrap inside the first window.
      mockFeedback.call(doCmd('nextObject'))
          .expectSpeech('third', 'Button')

          // Wrap.
          .call(doCmd('nextObject'))
          .expectSpeech('second', 'Button', 'first, window')

          // Wrap.
          .call(doCmd('previousObject'))
          .expectSpeech('third', 'Button')

          .call(() => {
            // Link the two "windows" with next/previous focus.
            Object.defineProperty(
                window1, 'nextWindowFocus', {get: () => window2});
            Object.defineProperty(
                window2, 'previousWindowFocus', {get: () => window1});
          })

          // window1 -> window2.
          .call(doCmd('nextObject'))
          .expectSpeech('fifth', 'Button', 'fourth, window')

          // window2 -> window1.
          .call(doCmd('previousObject'))
          .expectSpeech('third', 'Button', 'first, window')

          .call(() => {
            // Link the two "windows" with next/previous focus in a slightly
            // different way.
            Object.defineProperty(
                window1, 'previousWindowFocus', {get: () => window2});
            Object.defineProperty(
                window2, 'nextWindowFocus', {get: () => window1});
          })

          .call(doCmd('previousObject'))
          .expectSpeech('second', 'Button')

          // window1 -> window2.
          .call(doCmd('previousObject'))
          .expectSpeech('sixth', 'Button', 'fourth, window')

          // window2 -> window1.
          .call(doCmd('nextObject'))
          .expectSpeech('second', 'Button', 'first, window');

      await mockFeedback.replay();
    });

// TODO(crbug.com/260291606): flaky.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'DISABLED_GestureOnPopUpButton',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <select><option>apple</option><option>banana</option></select>
  `;
      await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('Button', 'has pop up')
          .call(doGesture(Gesture.CLICK))
          .expectSpeech('Button', 'has pop up', 'Expanded')
          .call(doGesture(Gesture.SWIPE_DOWN1))
          .expectSpeech('banana')
          .call(doGesture(Gesture.SWIPE_UP1))
          .expectSpeech('apple');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'NestedImages', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
<div>start</div>
<div aria-label="first"><div aria-label="second"></div></div>
<div aria-label="third"><div aria-label="fourth"></div></div>
<div>end</div>
`;
  const root = await this.runWithLoadedTree(site);
  // Fake the divs as images. This nested structure isn't possible with ordinary
  // html.
  const img1 = root.children[1];
  const img2 = img1.children[0];
  const img3 = root.children[2];
  Object.defineProperty(img1, 'isImage', {get: () => true});
  Object.defineProperty(img2, 'isImage', {get: () => true});
  // Clears the name.
  Object.defineProperty(img2, 'name', {get: () => ''});
  Object.defineProperty(img3, 'isImage', {get: () => true});
  Object.defineProperty(img3, 'role', {get: () => RoleType.IMAGE});

  // Linear nav should visit the outer image only.
  mockFeedback.call(doCmd('nextObject'))
      .expectSpeech('first')

      // Visits the image which has text inside. This one has an image role, so
      // is spoken as one.
      .call(doCmd('nextObject'))
      .expectSpeech('third', 'Image')

      .call(doCmd('nextObject'))
      .expectSpeech('end');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxBackgroundTest', 'ToggleSpeechWithAnnouncement',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(Documents.autofocus + Documents.button);

      // The test will not finish unless this method is called.
      this.addCallbackPostMethod(
          TtsBackground, 'toggleSpeechWithAnnouncement', this.newCallback(),
          () => true /** remove callback */);

      mockFeedback.call(doCmd('toggleSpeechOnOrOff')).expectSpeech(/.*off.*/);
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxBackgroundTest', 'CanvasHasImageData', async function() {
  const site = `
    <canvas id=myCanvas></canvas>
    <script>
      const c = document.getElementById("myCanvas");
      const ctx = c.getContext("2d");
      ctx.beginPath();
      ctx.arc(95, 50, 40, 0, 2 * Math.PI);
      ctx.stroke();
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  const canvas = root.find({role: 'canvas'});
  canvas.getImageData(0, 0);
  await new Promise(r => {
    canvas.addEventListener(EventType.IMAGE_FRAME_UPDATED, r);
  });
  assertNotEquals('', canvas.imageDataUrl);

  // Repeated calls should continue to result in events.
  canvas.getImageData(0, 0);
  await new Promise(r => {
    canvas.addEventListener(EventType.IMAGE_FRAME_UPDATED, r);
  });
  assertNotEquals('', canvas.imageDataUrl);
});

AX_TEST_F('ChromeVoxBackgroundTest', 'NestedEmptyClickable', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
<div>start</div>
<div tabindex=0><div aria-label="label" tabindex=0></div></div>
<div>end</div>
`;
  const root = await this.runWithLoadedTree(site);
  const outer = root.children[1];
  const inner = outer.children[0];

  Object.defineProperty(outer, 'clickable', {get: () => true});
  Object.defineProperty(inner, 'clickable', {get: () => true});

  Object.defineProperty(outer, 'name', {get: () => undefined});

  // Linear nav should visit the inner div only.
  mockFeedback
      .expectSpeech('start')

      .call(doCmd('nextObject'))
      .expectSpeech('label')

      .call(doCmd('nextObject'))
      .expectSpeech('end');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxBackgroundTest', 'CustomTabList', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(Documents.customTabList);
  const tabList = root.find({role: RoleType.TAB_LIST});
  assertNotNullNorUndefined(tabList);
  const tabs = root.findAll({role: RoleType.TAB});
  assertEquals(2, tabs.length, 'Expected two tabs');

  mockFeedback.call(() => tabs[1].doDefault())
      .expectSpeech(/.*, tab/)
      .expectSpeech(/[0-9]+ of [0-9]+/)
      .expectSpeech('Selected');
  await mockFeedback.replay();
});

// TODO(crbug.com/361584737) TODO(crbug.com/369705510): Test is flaky.
AX_TEST_F(
    'ChromeVoxBackgroundTest', 'DISABLED_OpenKeyboardShortcuts',
    async function() {
      const mockFeedback = this.createMockFeedback();
      mockFeedback.call(doCmd('openKeyboardShortcuts'))
          .expectSpeech('Search shortcuts')
          .replay();
    });
