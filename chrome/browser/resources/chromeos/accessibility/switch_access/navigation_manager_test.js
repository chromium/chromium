// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the navigation manager. */
SwitchAccessNavigationManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  setUp() {
    this.navigator = NavigationManager.instance;
    BackButtonNode
        .locationForTesting = {top: 10, left: 10, width: 20, height: 20};
  }

  moveToPageContents(pageContents) {
    const cache = new SACache();
    if (!SwitchAccessPredicate.isGroup(pageContents, null, cache)) {
      pageContents =
          new AutomationTreeWalker(pageContents, constants.Dir.FORWARD, {
            visit: (node) => SwitchAccessPredicate.isGroup(node, null, cache)
          })
              .next()
              .node;
    }
    assertNotNullNorUndefined(
        pageContents, 'Could not find group corresponding to page contents');
    this.navigator.moveTo_(pageContents);
    NavigationManager.enterGroup();
  }
};

function currentNode() {
  return NavigationManager.instance.node_;
}

TEST_F('SwitchAccessNavigationManagerTest', 'MoveTo', function() {
  const website = `<div id="outerGroup">
                     <div id="group">
                       <input type="text">
                       <input type="range">
                     </div>
                     <button></button>
                   </div>`;
  this.runWithLoadedTree(website, (desktop) => {
    const textFields =
        desktop.findAll({role: chrome.automation.RoleType.TEXT_FIELD});
    assertEquals(2, textFields.length, 'Should be exactly 2 text fields.');
    const omnibar = textFields[0];
    const textInput = textFields[1];
    const sliders = desktop.findAll({role: chrome.automation.RoleType.SLIDER});
    assertEquals(1, sliders.length, 'Should be exactly 1 slider.');
    const slider = sliders[0];
    const group = this.findNodeById('group');
    const outerGroup = this.findNodeById('outerGroup');

    this.navigator.moveTo_(omnibar);
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD, this.navigator.node_.role,
        'Did not successfully move to the omnibar');
    assertFalse(
        this.navigator.group_.isEquivalentTo(group),
        'Omnibar is in the group in page contents somehow');
    assertFalse(
        this.navigator.group_.isEquivalentTo(outerGroup),
        'Omnibar is in the outer group in page contents somehow');
    const grandGroup = this.navigator.history_.peek().group;
    assertFalse(
        grandGroup.isEquivalentTo(group),
        'Group stack contains the group from page contents');
    assertFalse(
        grandGroup.isEquivalentTo(outerGroup),
        'Group stack contains the outer group from page contents');

    this.navigator.moveTo_(textInput);
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD, this.navigator.node_.role,
        'Did not successfully move to the text input');
    assertTrue(
        this.navigator.group_.isEquivalentTo(group),
        'Group node was not successfully populated');
    assertTrue(
        this.navigator.history_.peek().group.isEquivalentTo(outerGroup),
        'History was not built properly');

    this.navigator.moveTo_(slider);
    assertEquals(
        chrome.automation.RoleType.SLIDER, this.navigator.node_.role,
        'Did not successfully move to the slider');

    this.navigator.moveTo_(group);
    assertTrue(this.navigator.node_.isGroup(), 'Current node is not a group');
    assertTrue(
        this.navigator.node_.isEquivalentTo(group),
        'Did not find the right group');
  });
});

TEST_F('SwitchAccessNavigationManagerTest', 'JumpTo', function() {
  const website = `<div id="group1">
                     <input type="text">
                     <button></button>
                   </div>
                   <div id="group2">
                     <button></button>
                     <button></button>
                   </div>`;
  this.runWithLoadedTree(website, (desktop) => {
    const textInput =
        desktop.findAll({role: chrome.automation.RoleType.TEXT_FIELD})[1];
    assertNotNullNorUndefined(textInput, 'Text field is undefined');
    const group1 = this.findNodeById('group1');
    const group2 = this.findNodeById('group2');

    this.navigator.moveTo_(textInput);
    const textInputNode = this.navigator.node_;
    assertEquals(
        chrome.automation.RoleType.TEXT_FIELD, textInputNode.role,
        'Did not successfully move to the text input');
    assertTrue(
        this.navigator.group_.isEquivalentTo(group1),
        'Group is initialized in an unexpected manner');

    this.navigator.jumpTo_(BasicRootNode.buildTree(group2));
    assertFalse(
        this.navigator.group_.isEquivalentTo(group1), 'Jump did nothing');
    assertTrue(
        this.navigator.group_.isEquivalentTo(group2),
        'Jumped to the wrong group');

    this.navigator.exitGroup_();
    assertTrue(
        this.navigator.group_.isEquivalentTo(group1),
        'Did not jump back to the right group.');
  });
});

TEST_F('SwitchAccessNavigationManagerTest', 'SelectButton', function() {
  const website = `<button id="test" aria-pressed=false>First Button</button>
      <button>Second Button</button>
      <script>
        let state = false;
        let button = document.getElementById("test");
        button.onclick = () => {
          state = !state;
          button.setAttribute("aria-pressed", state);
        };
      </script>`;

  this.runWithLoadedTree(website, function(pageContents) {
    this.moveToPageContents(pageContents);

    const node = currentNode().automationNode;
    assertNotNullNorUndefined(node, 'Node is invalid');
    assertEquals(node.name, 'First Button', 'Did not find the right node');

    node.addEventListener(
        chrome.automation.EventType.CHECKED_STATE_CHANGED,
        this.newCallback((event) => {
          assertEquals(
              node.name, event.target.name,
              'Checked state changed on unexpected node');
        }));

    NavigationManager.instance.node_.performAction('select');
  }, {returnPage: true});
});

TEST_F('SwitchAccessNavigationManagerTest', 'EnterGroup', function() {
  const website = `<div id="group">
                     <button></button>
                     <button></button>
                   </div>
                   <input type="range">`;
  this.runWithLoadedTree(website, (desktop) => {
    const targetGroup = this.findNodeById('group');
    this.navigator.moveTo_(targetGroup);

    const originalGroup = this.navigator.group_;
    assertEquals(
        this.navigator.node_.automationNode.htmlAttributes.id, 'group',
        'Did not move to group properly');

    NavigationManager.enterGroup();
    assertEquals(
        chrome.automation.RoleType.BUTTON, this.navigator.node_.role,
        'Current node is not a button');
    assertTrue(
        this.navigator.group_.isEquivalentTo(targetGroup),
        'Target group was not entered');

    this.navigator.exitGroup_();
    assertTrue(
        originalGroup.equals(this.navigator.group_),
        'Did not move back to the original group');
  });
});

TEST_F('SwitchAccessNavigationManagerTest', 'MoveForward', function() {
  const website = `<div>
                     <button id="button1"></button>
                     <button id="button2"></button>
                     <button id="button3"></button>
                   </div>`;
  this.runWithLoadedTree(website, (desktop) => {
    this.navigator.moveTo_(this.findNodeById('button1'));
    const button1 = this.navigator.node_;
    assertFalse(
        button1 instanceof BackButtonNode,
        'button1 should not be a BackButtonNode');
    assertEquals(
        'button1', button1.automationNode.htmlAttributes.id,
        'Current node is not button1');

    NavigationManager.moveForward();
    assertFalse(
        button1.equals(this.navigator.node_),
        'Still on button1 after moveForward()');
    const button2 = this.navigator.node_;
    assertFalse(
        button2 instanceof BackButtonNode,
        'button2 should not be a BackButtonNode');
    assertEquals(
        'button2', button2.automationNode.htmlAttributes.id,
        'Current node is not button2');

    NavigationManager.moveForward();
    assertFalse(
        button1.equals(this.navigator.node_),
        'Unexpected navigation to button1');
    assertFalse(
        button2.equals(this.navigator.node_),
        'Still on button2 after moveForward()');
    const button3 = this.navigator.node_;
    assertFalse(
        button3 instanceof BackButtonNode,
        'button3 should not be a BackButtonNode');
    assertEquals(
        'button3', button3.automationNode.htmlAttributes.id,
        'Current node is not button3');

    NavigationManager.moveForward();
    assertTrue(
        this.navigator.node_ instanceof BackButtonNode,
        'BackButtonNode should come after button3');

    NavigationManager.moveForward();
    assertTrue(
        button1.equals(this.navigator.node_),
        'button1 should come after the BackButtonNode');
  });
});

TEST_F('SwitchAccessNavigationManagerTest', 'MoveBackward', function() {
  const website = `<div>
                     <button id="button1"></button>
                     <button id="button2"></button>
                     <button id="button3"></button>
                   </div>`;
  this.runWithLoadedTree(website, (desktop) => {
    this.navigator.moveTo_(this.findNodeById('button1'));
    const button1 = this.navigator.node_;
    assertFalse(
        button1 instanceof BackButtonNode,
        'button1 should not be a BackButtonNode');
    assertEquals(
        'button1', button1.automationNode.htmlAttributes.id,
        'Current node is not button1');

    NavigationManager.moveBackward();
    assertTrue(
        this.navigator.node_ instanceof BackButtonNode,
        'BackButtonNode should come before button1');

    NavigationManager.moveBackward();
    assertFalse(
        button1.equals(this.navigator.node_),
        'Unexpected navigation to button1');
    const button3 = this.navigator.node_;
    assertFalse(
        button3 instanceof BackButtonNode,
        'button3 should not be a BackButtonNode');
    assertEquals(
        'button3', button3.automationNode.htmlAttributes.id,
        'Current node is not button3');

    NavigationManager.moveBackward();
    assertFalse(
        button3.equals(this.navigator.node_),
        'Still on button3 after moveBackward()');
    assertFalse(button1.equals(this.navigator.node_), 'Skipped button2');
    const button2 = this.navigator.node_;
    assertFalse(
        button2 instanceof BackButtonNode,
        'button2 should not be a BackButtonNode');
    assertEquals(
        'button2', button2.automationNode.htmlAttributes.id,
        'Current node is not button2');

    NavigationManager.moveBackward();
    assertTrue(
        button1.equals(this.navigator.node_),
        'button1 should come before button2');
  });
});

TEST_F(
    'SwitchAccessNavigationManagerTest', 'NodeUndefinedBeforeTreeChangeRemoved',
    function() {
      const website = `<div>
                     <button id="button1"></button>
                   </div>`;
      this.runWithLoadedTree(website, (desktop) => {
        this.navigator.moveTo_(this.findNodeById('button1'));
        const button1 = this.navigator.node_;
        assertFalse(
            button1 instanceof BackButtonNode,
            'button1 should not be a BackButtonNode');
        assertEquals(
            'button1', button1.automationNode.htmlAttributes.id,
            'Current node is not button1');

        // Simulate the underlying node's deletion. Note that this is different
        // than an orphaned node (which can have a valid AutomationNode
        // instance, but no backing C++ object, so attributes returned like role
        // are undefined).
        NavigationManager.instance.node_.baseNode_ = undefined;

        // Tree change removed gets sent by C++ after the tree has already
        // applied changes (so this comes after the above clearing).
        NavigationManager.instance.onTreeChange_(
            {type: chrome.automation.TreeChangeType.NODE_REMOVED});
      });
    });
