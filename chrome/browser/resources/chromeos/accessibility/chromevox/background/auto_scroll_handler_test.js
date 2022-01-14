// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for AutoScrollHandler.
 */
ChromeVoxAutoScrollHandlerTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    super.setUp();
    window.EventType = chrome.automation.EventType;

    this.forceContextualLastOutput();
  }

  runWithFakeArcSimpleScrollable(callback) {
    // This simulates a scrolling behavior of Android scrollable, where when a
    // scroll action is performed, a new item is added to the list.
    const site = `
      <div id="div" role="list">
        <p>1st item</p>
        <p>2nd item</p>
        <p>3rd item</p>
      </div>
      <script>
        let i = 4;
        const list = document.getElementById('div');
        list.addEventListener('click', () => {
          const child = document.createElement('p');
          child.innerHTML = i + "th item";
          list.append(child);
          i++;
        });
      </script>
      `;
    this.runWithLoadedTree(site, function(root) {
      const list = root.firstChild;
      Object.defineProperty(list, 'focusable', {get: () => false});
      Object.defineProperty(list, 'scrollable', {get: () => true});
      Object.defineProperty(list, 'standardActions', {
        get: () =>
            [chrome.automation.ActionType.SCROLL_FORWARD,
             chrome.automation.ActionType.SCROLL_BACKWARD]
      });

      // Create a fake addEventListener to dispatch an event listener of
      // SCROLL_POSITION_CHANGED.
      let eventListener;
      const originalAddEventListenerFunc = list.addEventListener.bind(list);
      list.addEventListener = (eventType, callback, capture) => {
        if (eventType === EventType.SCROLL_POSITION_CHANGED) {
          eventListener = callback;
          return;
        } else if (
            eventType === EventType.SCROLL_HORIZONTAL_POSITION_CHANGED ||
            eventType === EventType.SCROLL_VERTICAL_POSITION_CHANGED) {
          // Do nothing to prevent catching scroll events dispatched from web.
          return;
        }
        originalAddEventListenerFunc(eventType, callback, capture);
      };

      // Create a fake scrollForward and scrollBackward actions.
      const fakeScrollFunc = (cb) => {
        const numChildrenBeforeScroll = list.children.length;
        const listener = (ev) => {
          if (list.children.length === numChildrenBeforeScroll) {
            return;
          }
          list.removeEventListener(EventType.CHILDREN_CHANGED, listener, true);
          eventListener();
        };
        list.addEventListener(EventType.CHILDREN_CHANGED, listener, true);

        // Invoke 'click' event on the list, which updates the list items.
        list.doDefault();
        cb(true);
      };
      list.scrollForward = fakeScrollFunc;
      list.scrollBackward = fakeScrollFunc;

      callback(root);
    });
  }
};

TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'DontScrollInSameScrollable', function() {
      this.runWithFakeArcSimpleScrollable(function(root) {
        const handler = new AutoScrollHandler();

        const list = root.firstChild;
        const firstItemCursor = cursors.Range.fromNode(list.firstChild);
        const lastItemCursor = cursors.Range.fromNode(list.lastChild);

        ChromeVoxState.instance.navigateToRange(firstItemCursor);

        assertTrue(handler.onCommandNavigation(
            lastItemCursor, constants.Dir.FORWARD, /*pred=*/ null,
            /*speechProps=*/ null));
      });
    });

TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'PreventMultipleScrolling', function() {
      this.runWithFakeArcSimpleScrollable(function(root) {
        const handler = new AutoScrollHandler();

        const list = root.firstChild;
        const rootCursor = cursors.Range.fromNode(root);
        const firstItemCursor = cursors.Range.fromNode(list.firstChild);
        const lastItemCursor = cursors.Range.fromNode(list.lastChild);

        ChromeVoxState.instance.navigateToRange(lastItemCursor);

        // Make scrolling action void, so that the second invocation should be
        // ignored.
        list.scrollForward = () => {};

        assertFalse(handler.onCommandNavigation(
            rootCursor, constants.Dir.FORWARD, /*pred=*/ null,
            /*speechProps=*/ null));

        assertFalse(handler.onCommandNavigation(
            firstItemCursor, constants.Dir.FORWARD, /*pred=*/ null,
            /*speechProps=*/ null));
      });
    });

TEST_F('ChromeVoxAutoScrollHandlerTest', 'ScrollForward', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithFakeArcSimpleScrollable(function(root) {
    mockFeedback.expectSpeech('1st item')
        .call(doCmd('nextObject'))
        .expectSpeech('2nd item')
        .call(doCmd('nextObject'))
        .expectSpeech('3rd item')
        .call(doCmd('nextObject'))
        .expectSpeech('4th item')
        .call(doCmd('nextObject'))
        .expectSpeech('5th item')
        .replay();
  });
});
