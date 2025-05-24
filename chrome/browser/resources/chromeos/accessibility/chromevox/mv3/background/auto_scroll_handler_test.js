// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for AutoScrollHandler.
 */
ChromeVoxAutoScrollHandlerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    globalThis.EventType = chrome.automation.EventType;
    globalThis.RoleType = chrome.automation.RoleType;
    this.forceContextualLastOutput();
  }

  /** @return {AutomationNode} */
  async runWithFakeArcSimpleScrollable() {
    // This simulates a scrolling behavior of Android scrollable, where when a
    // scroll action is performed, a new item is added to the list.
    const site = `
      <div id="div" role="list">
        <p>1st item</p>
        <p>2nd item</p>
        <p>3rd item</p>
      </div>
      <p>hello</p><p>world</p>
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
    return await this.runWithFakeScrollable(site, {
      numChildrenBeforeScroll_: -1,
      beforeScroll: list => {
        this.numChildrenBeforeScroll_ = list.children.length;
      },
      scrollFinished: list =>
          list.children.length !== this.numChildrenBeforeScroll_,
    });
  }

  /** @return {chrome.automation.AutomationNode} */
  async runWithFakeArcRecyclerView() {
    // This simulates a scrolling behavior in Android RecyclerView, where when a
    // scroll action is performed, the previously visible items disappear and
    // the new items are added to the list.
    const site = `
      <div id="div" role="list">
        <p tabindex="0">1st item</p>
        <p tabindex="0">2nd item</p>
      </div>
      <p>unrelated content</p>
      <script>
        let isShowingFirst = true;
        const list = document.getElementById('div');
        list.addEventListener('click', () => {
          while (list.firstChild) {
            list.removeChild(list.firstChild);
          }
          const child1 = document.createElement('p');
          child1.innerHTML = isShowingFirst ? '3rd item' : '1st item';
          child1.tabIndex = 0;
          list.append(child1);
          child1.focus();
          const child2 = document.createElement('p');
          child2.innerHTML = isShowingFirst ? '4th item' : '2nd item';
          list.append(child2);
          isShowingFirst = !isShowingFirst;
        });
      </script>
      `;
    return await this.runWithFakeScrollable(site, {
      childrenBeforeScroll_: [],
      beforeScroll: list => {
        this.childrenBeforeScroll_ = list.children;
      },
      scrollFinished: list => list.children.length === 2 &&
          list.children[0] !== this.childrenBeforeScroll_[0] &&
          list.children[1] !== this.childrenBeforeScroll_[1],
    });
  }

  /**
   * Loads site, sets up fake scrollable object that behaves similar to ARC++,
   * and run the given callback.
   * Instead of actually scrolling, this injects 'click' action to the list, and
   * the document should handle click event to simulate the scrolling behavior.
   *
   * |scrolledPredicate| is used to determine when to call a event listener of
   * EventType.SCROLL_POSITION_CHANGED, because this event type is not
   * dispatched from web, we need to handle it explicitly.
   *
   * @param {string} site
   * @param {{beforeScroll: Function, scrollFinished: Function}}
   *     scrolledPredicate
   *   beforeScroll: called before performing a fake scrolling (click action).
   *   scrollFinished: return if the scrolling has finished and the event
   *     listener can be invoked.
   * @return {chrome.automation.AutomationNode}
   */
  async runWithFakeScrollable(site, scrolledPredicate) {
    const root = await this.runWithLoadedTree(site);
    const list = root.find({role: RoleType.LIST});
    Object.defineProperty(list, 'focusable', {get: () => false});
    Object.defineProperty(list, 'scrollable', {get: () => true});
    Object.defineProperty(list, 'standardActions', {
      get: () =>
          [chrome.automation.ActionType.SCROLL_FORWARD,
           chrome.automation.ActionType.SCROLL_BACKWARD],
    });
    AutoScrollHandler.instance.allowWebContentsForTesting_ = true;

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
    const fakeScrollFunc = cb => {
      scrolledPredicate.beforeScroll(list);
      const listener = ev => {
        if (!scrolledPredicate.scrollFinished(list)) {
          return;
        }
        list.removeEventListener(EventType.CHILDREN_CHANGED, listener, true);
        assertEquals('function', typeof eventListener);
        eventListener();
      };
      list.addEventListener(EventType.CHILDREN_CHANGED, listener, true);

      // Invoke 'click' event on the list, which updates the list items.
      list.doDefault();
      cb(true);
    };
    list.scrollForward = fakeScrollFunc;
    list.scrollBackward = fakeScrollFunc;

    return root;
  }
};

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'DontScrollInSameScrollable',
    async function() {
      const root = await this.runWithFakeArcSimpleScrollable();
      const handler = new AutoScrollHandler();

      const list = root.find({role: RoleType.LIST});
      const firstItemCursor = CursorRange.fromNode(list.firstChild);
      const lastItemCursor = CursorRange.fromNode(list.lastChild);

      ChromeVoxRange.navigateTo(firstItemCursor);

      assertTrue(handler.onCommandNavigation(
          lastItemCursor, constants.Dir.FORWARD, /*pred=*/ null,
          /*speechProps=*/ null));
    });

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'PreventMultipleScrolling',
    async function() {
      const root = await this.runWithFakeArcSimpleScrollable();
      const handler = new AutoScrollHandler();
      handler.allowWebContentsForTesting_ = true;

      const list = root.find({role: RoleType.LIST});
      const rootCursor = CursorRange.fromNode(root);
      const firstItemCursor = CursorRange.fromNode(list.firstChild);
      const lastItemCursor = CursorRange.fromNode(list.lastChild);

      ChromeVoxRange.navigateTo(lastItemCursor);

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

AX_TEST_F('ChromeVoxAutoScrollHandlerTest', 'ScrollForward', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithFakeArcSimpleScrollable();
  mockFeedback.expectSpeech('1st item')
      .call(doCmd('nextObject'))
      .expectSpeech('2nd item')
      .call(doCmd('nextObject'))
      .expectSpeech('3rd item')
      .call(doCmd('nextObject'))
      .expectSpeech('4th item')
      .call(doCmd('nextObject'))
      .expectSpeech('5th item');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'ScrollForwardReturnsFalse',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithFakeArcSimpleScrollable();
      const list = root.find({role: RoleType.LIST});
      list.scrollForward = callback => callback(false);

      mockFeedback.expectSpeech('1st item')
          .call(doCmd('nextObject'))
          .expectSpeech('2nd item')
          .call(doCmd('nextObject'))
          .expectSpeech('3rd item')
          .call(doCmd('nextObject'))
          .expectSpeech('hello')
          .call(doCmd('nextObject'))
          .expectSpeech('world');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'RecyclerViewByObject', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithFakeArcRecyclerView();
      mockFeedback.expectSpeech('1st item')
          .call(doCmd('nextObject'))
          .expectSpeech('2nd item')
          .call(doCmd('nextObject'))  // scroll forward
          .expectSpeech('3rd item')
          .call(doCmd('previousObject'))  // scroll backward
          .expectSpeech('2nd item');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'RecyclerViewByWord', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithFakeArcRecyclerView();
      mockFeedback.expectSpeech('1st item')
          .call(doCmd('nextObject'))
          .expectSpeech('2nd item')
          .call(doCmd('nextWord'))
          .expectSpeech('item')
          .call(doCmd('nextWord'))  // scroll forward
          .expectSpeech('3rd')
          .call(doCmd('previousWord'))  // scroll backward
          .expectSpeech('item')
          .call(doCmd('previousWord'))
          .expectSpeech('2nd');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'RecyclerViewByCharacter',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithFakeArcRecyclerView();
      mockFeedback.expectSpeech('1st item')
          .call(doCmd('nextObject'))
          .expectSpeech('2nd item')
          .call(doCmd('nextWord'))
          .expectSpeech('item')
          .call(doCmd('nextCharacter'))
          .expectSpeech('t')
          .call(doCmd('nextCharacter'))
          .expectSpeech('e')
          .call(doCmd('nextCharacter'))
          .expectSpeech('m')
          .call(doCmd('nextCharacter'))  // scroll forward
          .expectSpeech('3')
          .call(doCmd('nextCharacter'))
          .expectSpeech('r')
          .call(doCmd('previousCharacter'))
          .expectSpeech('3')
          .call(doCmd('previousCharacter'))  // scroll backward
          .expectSpeech('m')
          .call(doCmd('previousCharacter'))
          .expectSpeech('e');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'RecyclerViewByPredicate',
    async function() {
      // TODO(hirokisato): This test fails without '<p>unrelated content</p>' in
      // the tree, because the next item of '2nd item' without scrolling is '1st
      // item', and the scrollable is LCA, so auto scrolling is not invoked. We
      // should fix this corner case.
      const mockFeedback = this.createMockFeedback();
      await this.runWithFakeArcRecyclerView();
      mockFeedback.expectSpeech('1st item')
          .call(doCmd('nextSimilarItem'))
          .expectSpeech('2nd item')
          .call(doCmd('nextSimilarItem'))  // scroll forward
          .expectSpeech('3rd item')
          .call(doCmd('previousSimilarItem'))  // scroll backward
          .expectSpeech('2nd item');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxAutoScrollHandlerTest', 'DontScrollInWebContents',
    async function() {
      const mockFeedback = this.createMockFeedback();
      // Tests for fix to b/284050731.
      const site = `
        <div style="width:100px">
            <a href="#" autofocus>Before</a>
            <div role="list"
                 style="display: flex; overflow-x: scroll; overflow-y: hidden;">
                <div role="listitem" style="flex:0 0 150px">
                    <a href="#">Card 1</a>
                </div>
                <div role="listitem" style="flex:0 0 150px">
                    <a href="#">Card 2</a>
                </div>
            </div>
            <a href="#">After</a>
        </div>`;
      await this.runWithLoadedTree(site);
      mockFeedback.expectSpeech('Before')
          .call(doCmd('nextObject'))
          .expectSpeech('Card 1')
          .call(doCmd('nextObject'))
          .expectSpeech('Card 2')
          .call(doCmd('nextObject'))
          .expectSpeech('After');
      await mockFeedback.replay();
    });
