// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for Live Regions.
 */
ChromeVoxLiveRegionsTest = class extends ChromeVoxE2ETest {
  async setUpDeferred() {
    await super.setUpDeferred();

    globalThis.EventType = chrome.automation.EventType;
    globalThis.RoleType = chrome.automation.RoleType;
    globalThis.TreeChangeType = chrome.automation.TreeChangeType;
  }

  /**
   * Simulates work done when users interact using keyboard, braille, or
   * touch.
   */
  simulateUserInteraction() {
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
  }
};


AX_TEST_F('ChromeVoxLiveRegionsTest', 'LiveRegionAddElement', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode = await this.runWithLoadedTree(`
      <h1>Document with live region</h1>
      <p id="live" aria-live="assertive"></p>
      <button id="go">Go</button>
      <script>
        document.getElementById('go').addEventListener('click', function() {
          document.getElementById('live').innerHTML = 'Hello, world';
        }, false);
      </script>
    `);
  const go = rootNode.find({role: RoleType.BUTTON});
  mockFeedback.call(go.doDefault.bind(go))
      .expectCategoryFlushSpeech('Hello, world');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'LiveRegionRemoveElement', async function() {
      const mockFeedback = this.createMockFeedback();
      const rootNode = await this.runWithLoadedTree(`
      <h1>Document with live region</h1>
      <p id="live" aria-live="assertive" aria-relevant="removals">Hello, world</p>
      <button id="go">Go</button>
      <script>
        document.getElementById('go').addEventListener('click', function() {
          document.getElementById('live').innerHTML = '';
        }, false);
      </script>
    `);
      const go = rootNode.find({role: RoleType.BUTTON});
      go.doDefault();
      mockFeedback.expectCategoryFlushSpeech('removed:')
          .expectQueuedSpeech('Hello, world');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'LiveRegionChangeAtomic', async function() {
      LiveRegions.LIVE_REGION_QUEUE_TIME_MS = 0;
      const mockFeedback = this.createMockFeedback();
      const rootNode = await this.runWithLoadedTree(`
      <div id="live" aria-live="assertive" aria-atomic="true">
        <div></div><div>Bravo</div><div></div>
      </div>
      <button id="go">Go</button>
      <script>
        document.getElementById('go').addEventListener('click', function() {
          document.querySelectorAll('div div')[2].textContent = 'Charlie';
          document.querySelectorAll('div div')[0].textContent = 'Alpha';
        }, false);
      </script>
    `);
      const go = rootNode.find({role: RoleType.BUTTON});
      mockFeedback.call(go.doDefault.bind(go))
          .expectCategoryFlushSpeech('Alpha Bravo Charlie');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'LiveRegionChangeAtomicText', async function() {
      LiveRegions.LIVE_REGION_QUEUE_TIME_MS = 0;
      const mockFeedback = this.createMockFeedback();
      const rootNode = await this.runWithLoadedTree(`
      <h1 aria-atomic="true" id="live"aria-live="assertive">foo</h1>
      <button id="go">go</button>
      <script>
        document.getElementById('go').addEventListener('click', function(e) {
          document.getElementById('live').innerText = 'bar';
        });
      </script>
    `);
      const go = rootNode.find({role: RoleType.BUTTON});
      mockFeedback.call(go.doDefault.bind(go))
          .expectCategoryFlushSpeech('bar', 'Heading 1');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'LiveRegionChangeImageAlt', async function() {
      // Note that there is a live region outputted as a result of page load;
      // the test expects a live region announcement after a click on the
      // button, but the LiveRegions module has a half second filter for live
      // region announcements on the same node. Set that timeout to 0 to prevent
      // flakeyness.
      LiveRegions.LIVE_REGION_QUEUE_TIME_MS = 0;
      const mockFeedback = this.createMockFeedback();
      const rootNode = await this.runWithLoadedTree(`
      <div id="live" aria-live="assertive">
        <img id="img" src="#" alt="Before">
      </div>
      <button id="go">Go</button>
      <script>
        document.getElementById('go').addEventListener('click', function() {
          document.getElementById('img').setAttribute('alt', 'After');
        }, false);
      </script>
    `);
      const go = rootNode.find({role: RoleType.BUTTON});
      mockFeedback.call(go.doDefault.bind(go))
          .expectCategoryFlushSpeech('After');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxLiveRegionsTest', 'LiveRegionThenFocus', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode = await this.runWithLoadedTree(`
      <div id="live" aria-live="assertive"></div>
      <button id="go">Go</button>
      <button id="focus">Focus</button>
      <script>
        document.getElementById('go').addEventListener('click', function() {
          document.getElementById('live').textContent = 'Live';
   setTimeout(function() {
            document.getElementById('focus').focus();
          }, 50);
        }, false);
      </script>
    `);
  // Due to the above timing component, the live region can come either
  // before or after the focus output. This depends on the EventBundle to
  // which we get the live region. It can either be in its own bundle or
  // be part of the bundle with the focus change. In either case, the
  // first event should be flushed; the second should either be queued (in
  // the case of the focus) or category flushed for the live region.
  let sawFocus = false;
  let sawLive = false;
  const focusOrLive = function(candidate) {
    sawFocus = candidate.text === 'Focus' || sawFocus;
    sawLive = candidate.text === 'Live' || sawLive;
    if (sawFocus && sawLive) {
      return candidate.queueMode !== QueueMode.FLUSH;
    } else if (sawFocus || sawLive) {
      return candidate.queueMode === QueueMode.FLUSH;
    }
  };
  const go = rootNode.find({role: RoleType.BUTTON});
  mockFeedback.call(this.simulateUserInteraction)
      .call(go.doDefault.bind(go))
      .expectSpeech(focusOrLive)
      .expectSpeech(focusOrLive);
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxLiveRegionsTest', 'FocusThenLiveRegion', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode = await this.runWithLoadedTree(`
      <div id="live" aria-live="assertive"></div>
      <button id="go">Go</button>
      <button id="focus">Focus</button>
      <script>
        document.getElementById('go').addEventListener('click', function() {
          document.getElementById('focus').focus();
   setTimeout(function() {
            document.getElementById('live').textContent = 'Live';
          }, 200);
        }, false);
      </script>
    `);
  const go = rootNode.find({role: RoleType.BUTTON});
  mockFeedback.call(this.simulateUserInteraction)
      .call(go.doDefault.bind(go))
      .expectSpeech('Focus')
      .expectSpeech(candidate => {
        return candidate.text === 'Live' &&
            (candidate.queueMode === QueueMode.CATEGORY_FLUSH ||
             candidate.queueMode === QueueMode.QUEUE);
      });
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'LiveRegionCategoryFlush', async function() {
      // Adjust the live region queue time to be shorter (i.e. flushes happen
      // for live regions coming 1 ms in time). Also, can help with flakeyness.
      LiveRegions.LIVE_REGION_QUEUE_TIME_MS = 1;
      const mockFeedback = this.createMockFeedback();
      const rootNode = await this.runWithLoadedTree(`
      <div id="live1" aria-live="assertive"></div>
      <div id="live2" aria-live="assertive"></div>
      <button id="go">Go</button>
      <button id="focus">Focus</button>
      <script>
        document.getElementById('go').addEventListener('click', function() {
          document.getElementById('live1').textContent = 'Live1';
          setTimeout(function() {
            document.getElementById('live2').textContent = 'Live2';
          }, 1000);
        }, false);
      </script>
    `);
      const go = rootNode.find({role: RoleType.BUTTON});
      mockFeedback.call(go.doDefault.bind(go))
          .expectCategoryFlushSpeech('Live1')
          .expectCategoryFlushSpeech('Live2');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxLiveRegionsTest', 'SilentOnNodeChange', async function() {
  const mockFeedback = this.createMockFeedback();
  const rootNode = await this.runWithLoadedTree(`
    <p>start</p>
    <button>first</button>
    <div role="button" id="live" aria-live="assertive">
      hello!
    </div>
    <script>
      let live = document.getElementById('live');
      let pressed = true;
      setInterval(function() {
        live.setAttribute('aria-pressed', pressed);
        pressed = !pressed;
      }, 50);
    </script>
  `);
  const focusAfterNodeChange = setTimeout.bind(window, function() {
    root.firstChild.nextSibling.focus();
  }, 1000);
  mockFeedback.call(focusAfterNodeChange)
      .expectSpeech('hello!')
      .expectNextSpeechUtteranceIsNot('hello!')
      .expectNextSpeechUtteranceIsNot('hello!');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxLiveRegionsTest', 'SimulateTreeChanges', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <button></button>
    <div aria-live="assertive">
      <p>hello</p><p>there</p>
    </div>
  `);
  const live = new LiveRegions(ChromeVoxState.instance);
  const [t1, t2] = root.findAll({role: RoleType.STATIC_TEXT});
  mockFeedback.expectSpeech('hello there')
      .clearPendingOutput()
      .call(function() {
        live.onTreeChange({type: TreeChangeType.TEXT_CHANGED, target: t2});
        live.onTreeChange(
            {type: TreeChangeType.SUBTREE_UPDATE_END, target: t2});
      })
      .expectNextSpeechUtteranceIsNot('hello')
      .expectSpeech('there')
      .clearPendingOutput();
  mockFeedback
      .call(function() {
        live.onTreeChange({type: TreeChangeType.TEXT_CHANGED, target: t1});
        live.onTreeChange({type: TreeChangeType.TEXT_CHANGED, target: t2});
        live.onTreeChange(
            {type: TreeChangeType.SUBTREE_UPDATE_END, target: t2});
      })
      .expectSpeech('hello')
      .expectSpeech('there');
  await mockFeedback.replay();
});

// Flaky: https://crbug.com/945199
AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'DISABLED_LiveStatusOff', async function() {
      const mockFeedback = this.createMockFeedback();
      const rootNode = await this.runWithLoadedTree(`
    <div><input aria-live="off" type="text"></input></div>
    <script>
      let input = document.querySelector('input');
      let div = document.querySelector('div');
      let clicks = 0;
      div.addEventListener('click', () => {
        clicks++;
        if (clicks === 1) {
          input.value = 'bb';
          input.selectionStart = 2;
          input.selectionEnd = 2;
        } else if (clicks === 2) {
          input.value = 'bba';
          input.selectionStart = 3;
          input.selectionEnd = 3;
        }
      });
    </script>
  `);
      const input = root.find({role: RoleType.TEXT_FIELD});
      const clickInput = input.parent.doDefault.bind(input.parent);
      mockFeedback.call(input.focus.bind(input))
          .call(clickInput)
          .expectSpeech('bb')
          .clearPendingOutput()
          .call(clickInput)
          .expectNextSpeechUtteranceIsNot('bba')
          .expectSpeech('a');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'TreeChangeOnIgnoredNode', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <button></button>
    <script>
      const button = document.body.children[0];
      button.addEventListener('click', () => {
        const ignored = document.createElement('div');
        ignored.setAttribute('role', 'presentation');
        const alert = document.createElement('div');
        alert.setAttribute('role', 'alert');
        alert.textContent = 'hi';
        ignored.appendChild(alert);
        document.body.appendChild(ignored);
      });
    </script>
  `);
      const button = root.find({role: chrome.automation.RoleType.BUTTON});
      mockFeedback.call(button.doDefault.bind(button))
          .expectSpeech('Alert', 'hi');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxLiveRegionsTest', 'ShouldIgnoreLiveRegion', function() {
  const liveRegions = new LiveRegions(ChromeVoxState.instance);

  const mockParentNode = {};
  mockParentNode.root = {role: chrome.automation.RoleType.DESKTOP};
  mockParentNode.state = {};

  const mockNode = {};
  mockNode.role = chrome.automation.RoleType.ROOT_WEB_AREA;
  mockNode.root = mockNode;
  mockNode.parent = mockParentNode;
  mockNode.state = {};

  mockParentNode.role = chrome.automation.RoleType.WINDOW;
  assertFalse(liveRegions.shouldIgnoreLiveRegion_(mockNode));
  mockParentNode.state[chrome.automation.StateType.INVISIBLE] = true;
  assertTrue(liveRegions.shouldIgnoreLiveRegion_(mockNode));
});

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'LiveIgnoredToUnignored', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <button></button>
    <div aria-live="polite" style="display:none">hello</div>
    <div aria-live="polite" hidden>there</div>
    <script>
      const [button, div1, div2] = document.body.children;
      let clickCount = 0;
      button.addEventListener('click', () => {
        clickCount++;
        switch (clickCount) {
          case 1:
            div1.style.display = 'block';
            break;
          case 2:
            div2.hidden = false;
        }
      });
    </script>
  `);
      const button = root.find({role: chrome.automation.RoleType.BUTTON});
      mockFeedback.call(button.doDefault.bind(button))
          .expectSpeech('hello')
          .call(button.doDefault.bind(button))
          .expectSpeech('there');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLiveRegionsTest', 'AnnounceDesktopLiveRegionChanged',
    async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(``);

      const fakeEvent = containerLiveStatus => {
        return {
          target: {
            containerLiveStatus,
            name: containerLiveStatus,
            root: this.desktop_,
            children: [],
            standardActions: [],
            state: {},
            unclippedLocation: {},
            addEventListener() {},
            makeVisible() {},
            removeEventListener() {},
            setAccessibilityFocus() {},
          },
          type: EventType.LIVE_REGION_CHANGED,
        };
      };

      const onLiveRegionChanged = status => () =>
          DesktopAutomationInterface.instance.onLiveRegionChanged_(
              fakeEvent(status));

      mockFeedback.call(onLiveRegionChanged('assertive'))
          .expectSpeechWithQueueMode('assertive', QueueMode.CATEGORY_FLUSH)
          .call(onLiveRegionChanged('polite'))
          .expectSpeechWithQueueMode('polite', QueueMode.QUEUE);
      await mockFeedback.replay();
    });
