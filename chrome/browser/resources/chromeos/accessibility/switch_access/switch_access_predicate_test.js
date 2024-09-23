// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the Switch Access predicates. */
SwitchAccessPredicateTest = class extends SwitchAccessE2ETest {};

function fakeLoc(x) {
  return {left: x, top: x, width: x, height: x};
}

// This page has a 1:1 correlation between DOM nodes and accessibility nodes.
function testWebsite() {
  return `<div aria-description="upper1">
            <div aria-description="lower1">
              <button>leaf1</button>
              <p aria-description="leaf2">leaf2</p>
              <button>leaf3</button>
            </div>
            <div aria-description="lower2">
              <p aria-description="leaf4">leaf4</p>
              <button>leaf5</button>
            </div>
          </div>
          <div aria-description="upper2" role="button">
            <div aria-description="lower3" >
              <p aria-description="leaf6">leaf6</p>
              <p aria-description="leaf7">leaf7</p>`;
}

function getTree(loadedPage) {
  const root = loadedPage;
  assertTrue(
      root.role === chrome.automation.RoleType.ROOT_WEB_AREA,
      'We should receive the root web area');
  assertTrue(
      root.firstChild && root.firstChild.description === 'upper1',
      'First child should be upper1');

  const upper1 = root.firstChild;
  assertTrue(upper1 && upper1.description === 'upper1', 'Upper1 not found');
  const upper2 = upper1.nextSibling;
  assertTrue(upper2 && upper2.description === 'upper2', 'Upper2 not found');
  const lower1 = upper1.firstChild;
  assertTrue(lower1 && lower1.description === 'lower1', 'Lower1 not found');
  const lower2 = lower1.nextSibling;
  assertTrue(lower2 && lower2.description === 'lower2', 'Lower2 not found');
  const lower3 = upper2.firstChild;
  assertTrue(lower3 && lower3.description === 'lower3', 'Lower3 not found');
  const leaf1 = lower1.firstChild;
  assertTrue(leaf1 && leaf1.name === 'leaf1', 'Leaf1 not found');
  const leaf2 = leaf1.nextSibling;
  assertTrue(leaf2 && leaf2.description === 'leaf2', 'Leaf2 not found');
  const leaf3 = leaf2.nextSibling;
  assertTrue(leaf3 && leaf3.name === 'leaf3', 'Leaf3 not found');
  const leaf4 = lower2.firstChild;
  assertTrue(leaf4 && leaf4.description === 'leaf4', 'Leaf4 not found');
  const leaf5 = leaf4.nextSibling;
  assertTrue(leaf5 && leaf5.name === 'leaf5', 'Leaf5 not found');
  const leaf6 = lower3.firstChild;
  assertTrue(leaf6 && leaf6.description === 'leaf6', 'Leaf6 not found');
  const leaf7 = leaf6.nextSibling;
  assertTrue(leaf7 && leaf7.description === 'leaf7', 'Leaf7 not found');

  return {
    root,
    upper1,
    upper2,
    lower1,
    lower2,
    lower3,
    leaf1,
    leaf2,
    leaf3,
    leaf4,
    leaf5,
    leaf6,
    leaf7,
  };
}

AX_TEST_F('SwitchAccessPredicateTest', 'IsInteresting', async function() {
  const loadedPage = await this.runWithLoadedTree(testWebsite());
  const t = getTree(loadedPage);
  const cache = new SACache();

  // The scope is only used to verify the locations are not the same, and
  // since the buildTree function depends on isInteresting, pass in null
  // for the scope.
  assertTrue(
      SwitchAccessPredicate.isInteresting(t.root, null, cache),
      'Root should be interesting');
  assertTrue(
      SwitchAccessPredicate.isInteresting(t.upper1, null, cache),
      'Upper1 should be interesting');
  assertTrue(
      SwitchAccessPredicate.isInteresting(t.upper2, null, cache),
      'Upper2 should be interesting');
  assertTrue(
      SwitchAccessPredicate.isInteresting(t.lower1, null, cache),
      'Lower1 should be interesting');
  assertFalse(
      SwitchAccessPredicate.isInteresting(t.lower2, null, cache),
      'Lower2 should not be interesting');
  assertFalse(
      SwitchAccessPredicate.isInteresting(t.lower3, null, cache),
      'Lower3 should not be interesting');
  assertTrue(
      SwitchAccessPredicate.isInteresting(t.leaf1, null, cache),
      'Leaf1 should be interesting');
  assertFalse(
      SwitchAccessPredicate.isInteresting(t.leaf2, null, cache),
      'Leaf2 should not be interesting');
  assertTrue(
      SwitchAccessPredicate.isInteresting(t.leaf3, null, cache),
      'Leaf3 should be interesting');
  assertFalse(
      SwitchAccessPredicate.isInteresting(t.leaf4, null, cache),
      'Leaf4 should not be interesting');
  assertTrue(
      SwitchAccessPredicate.isInteresting(t.leaf5, null, cache),
      'Leaf5 should be interesting');
  assertFalse(
      SwitchAccessPredicate.isInteresting(t.leaf6, null, cache),
      'Leaf6 should not be interesting');
  assertFalse(
      SwitchAccessPredicate.isInteresting(t.leaf7, null, cache),
      'Leaf7 should not be interesting');
});

AX_TEST_F('SwitchAccessPredicateTest', 'IsGroup', async function() {
  const loadedPage = await this.runWithLoadedTree(testWebsite());
  const t = getTree(loadedPage);
  const cache = new SACache();

  // The scope is only used to verify the locations are not the same, and
  // since the buildTree function depends on isGroup, pass in null for
  // the scope.
  assertTrue(
      SwitchAccessPredicate.isGroup(t.root, null, cache),
      'Root should be a group');
  assertTrue(
      SwitchAccessPredicate.isGroup(t.upper1, null, cache),
      'Upper1 should be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.upper2, null, cache),
      'Upper2 should not be a group');
  assertTrue(
      SwitchAccessPredicate.isGroup(t.lower1, null, cache),
      'Lower1 should be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.lower2, null, cache),
      'Lower2 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.lower3, null, cache),
      'Lower3 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf1, null, cache),
      'Leaf1 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf2, null, cache),
      'Leaf2 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf3, null, cache),
      'Leaf3 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf4, null, cache),
      'Leaf4 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf5, null, cache),
      'Leaf5 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf6, null, cache),
      'Leaf6 should not be a group');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf7, null, cache),
      'Leaf7 should not be a group');
});

AX_TEST_F(
    'SwitchAccessPredicateTest', 'IsInterestingSubtree', async function() {
      const loadedPage = await this.runWithLoadedTree(testWebsite());
      const t = getTree(loadedPage);
      const cache = new SACache();

      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.root, cache),
          'Root should be an interesting subtree');
      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.upper1, cache),
          'Upper1 should be an interesting subtree');
      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.upper2, cache),
          'Upper2 should be an interesting subtree');
      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.lower1, cache),
          'Lower1 should be an interesting subtree');
      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.lower2, cache),
          'Lower2 should be an interesting subtree');
      assertFalse(
          SwitchAccessPredicate.isInterestingSubtree(t.lower3, cache),
          'Lower3 should not be an interesting subtree');
      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.leaf1, cache),
          'Leaf1 should be an interesting subtree');
      assertFalse(
          SwitchAccessPredicate.isInterestingSubtree(t.leaf2, cache),
          'Leaf2 should not be an interesting subtree');
      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.leaf3, cache),
          'Leaf3 should be an interesting subtree');
      assertFalse(
          SwitchAccessPredicate.isInterestingSubtree(t.leaf4, cache),
          'Leaf4 should not be an interesting subtree');
      assertTrue(
          SwitchAccessPredicate.isInterestingSubtree(t.leaf5, cache),
          'Leaf5 should be an interesting subtree');
      assertFalse(
          SwitchAccessPredicate.isInterestingSubtree(t.leaf6, cache),
          'Leaf6 should not be an interesting subtree');
      assertFalse(
          SwitchAccessPredicate.isInterestingSubtree(t.leaf7, cache),
          'Leaf7 should not be an interesting subtree');
    });

AX_TEST_F('SwitchAccessPredicateTest', 'IsActionable', async function() {
  const treeString =
      `<button style="position:absolute; top:-100px;">offscreen</button>
       <button disabled>disabled</button>
       <a href="https://www.google.com/" aria-label="link1">link1</a>
       <input type="text" aria-label="input1">input1</input>
       <button>button3</button>
       <input type="range" aria-label="slider" value=5 min=0 max=10>
       <ol><div id="clickable" role="listitem" onclick="2+2"></div></ol>
       <div id="div1"><p>p1</p></div>`;
  const loadedPage = await this.runWithLoadedTree(treeString);
  const cache = new SACache();

  const offscreenButton = this.findNodeByNameAndRole('offscreen', 'button');
  assertFalse(
      SwitchAccessPredicate.isActionable(offscreenButton, cache),
      'Offscreen objects should not be actionable');

  const disabledButton = this.findNodeByNameAndRole('disabled', 'button');
  assertFalse(
      SwitchAccessPredicate.isActionable(disabledButton, cache),
      'Disabled objects should not be actionable');

  assertFalse(
      SwitchAccessPredicate.isActionable(loadedPage, cache),
      'Root web area should not be directly actionable');

  const link1 = this.findNodeByNameAndRole('link1', 'link');
  assertTrue(
      SwitchAccessPredicate.isActionable(link1, cache),
      'Links should be actionable');

  const input1 = this.findNodeByNameAndRole('input1', 'textField');
  assertTrue(
      SwitchAccessPredicate.isActionable(input1, cache),
      'Inputs should be actionable');

  const button3 = this.findNodeByNameAndRole('button3', 'button');
  assertTrue(
      SwitchAccessPredicate.isActionable(button3, cache),
      'Buttons should be actionable');

  const slider = this.findNodeByNameAndRole('slider', 'slider');
  assertTrue(
      SwitchAccessPredicate.isActionable(slider, cache),
      'Sliders should be actionable');

  const clickable = this.findNodeById('clickable');
  assertTrue(
      SwitchAccessPredicate.isActionable(clickable, cache),
      'Clickable list items should be actionable');

  const div1 = this.findNodeById('div1');
  assertFalse(
      SwitchAccessPredicate.isActionable(div1, cache),
      'Divs should not generally be actionable');

  const p1 = this.findNodeByNameAndRole('p1', 'staticText');
  assertFalse(
      SwitchAccessPredicate.isActionable(p1, cache),
      'Static text should not generally be actionable');
});

AX_TEST_F(
    'SwitchAccessPredicateTest', 'IsActionableFocusableElements',
    async function() {
      const treeString = `<div id="noChildren" tabindex=0></div>
       <div id="oneInterestingChild" tabindex=0>
         <div>
           <div>
             <div>
               <button>button1</button>
             </div>
           </div>
         </div>
       </div>
       <div id="oneUninterestingChild" tabindex=0>
         <p>p1</p>
       </div>
       <div id="interestingChildren" tabindex=0>
         <button>button2</button>
         <button>button3</button>
       </div>
       <div id="uninterestingChildren" tabindex=0>
         <p>p2</p>
         <p>p3</p>
       </div>`;
      await this.runWithLoadedTree(treeString);
      const cache = new SACache();

      const noChildren = this.findNodeById('noChildren');
      assertTrue(
          SwitchAccessPredicate.isActionable(noChildren, cache),
          'Focusable element with no children should be actionable');

      const oneInterestingChild = this.findNodeById('oneInterestingChild');
      assertFalse(
          SwitchAccessPredicate.isActionable(oneInterestingChild, cache),
          'Focusable element with an interesting child should not be actionable');

      const interestingChildren = this.findNodeById('interestingChildren');
      assertFalse(
          SwitchAccessPredicate.isActionable(interestingChildren, cache),
          'Focusable element with interesting children should not be ' +
              'actionable');

      const oneUninterestingChild = this.findNodeById('oneUninterestingChild');
      assertTrue(
          SwitchAccessPredicate.isActionable(oneUninterestingChild, cache),
          'Focusable element with one uninteresting child should be ' +
              'actionable');

      const uninterestingChildren = this.findNodeById('uninterestingChildren');
      assertTrue(
          SwitchAccessPredicate.isActionable(uninterestingChildren, cache),
          'Focusable element with uninteresting children should be actionable');
    });

AX_TEST_F('SwitchAccessPredicateTest', 'LeafPredicate', async function() {
  const loadedPage = await this.runWithLoadedTree(testWebsite());
  const t = getTree(loadedPage);
  const cache = new SACache();

  // Start with root as scope
  let leaf = SwitchAccessPredicate.leaf(t.root, cache);
  assertFalse(leaf(t.root), 'Root should not be a leaf node');
  assertTrue(leaf(t.upper1), 'Upper1 should be a leaf node for root tree');
  assertTrue(leaf(t.upper2), 'Upper2 should be a leaf node for root tree');

  // Set upper1 as scope
  leaf = SwitchAccessPredicate.leaf(t.upper1, cache);
  assertFalse(leaf(t.upper1), 'Upper1 should not be a leaf for upper1 tree');
  assertTrue(leaf(t.lower1), 'Lower1 should be a leaf for upper1 tree');
  assertTrue(leaf(t.leaf4), 'leaf4 should be a leaf for upper1 tree');
  assertTrue(leaf(t.leaf5), 'leaf5 should be a leaf for upper1 tree');

  // Set lower1 as scope
  leaf = SwitchAccessPredicate.leaf(t.lower1, cache);
  assertFalse(leaf(t.lower1), 'Lower1 should not be a leaf for lower1 tree');
  assertTrue(leaf(t.leaf1), 'Leaf1 should be a leaf for lower1 tree');
  assertTrue(leaf(t.leaf2), 'Leaf2 should be a leaf for lower1 tree');
  assertTrue(leaf(t.leaf3), 'Leaf3 should be a leaf for lower1 tree');
});

AX_TEST_F('SwitchAccessPredicateTest', 'RootPredicate', async function() {
  const loadedPage = await this.runWithLoadedTree(testWebsite());
  const t = getTree(loadedPage);

  // Start with root as scope
  let root = SwitchAccessPredicate.root(t.root);
  assertTrue(root(t.root), 'Root should be a root of the root tree');
  assertFalse(root(t.upper1), 'Upper1 should not be a root of the root tree');
  assertFalse(root(t.upper2), 'Upper2 should not be a root of the root tree');

  // Set upper1 as scope
  root = SwitchAccessPredicate.root(t.upper1);
  assertTrue(root(t.upper1), 'Upper1 should be a root of the upper1 tree');
  assertFalse(root(t.lower1), 'Lower1 should not be a root of the upper1 tree');
  assertFalse(root(t.lower2), 'Lower2 should not be a root of the upper1 tree');

  // Set lower1 as scope
  root = SwitchAccessPredicate.root(t.lower1);
  assertTrue(root(t.lower1), 'Lower1 should be a root of the lower1 tree');
  assertFalse(root(t.leaf1), 'Leaf1 should not be a root of the lower1 tree');
  assertFalse(root(t.leaf2), 'Leaf2 should not be a root of the lower1 tree');
  assertFalse(root(t.leaf3), 'Leaf3 should not be a root of the lower1 tree');
});

AX_TEST_F('SwitchAccessPredicateTest', 'VisitPredicate', async function() {
  const loadedPage = await this.runWithLoadedTree(testWebsite());
  const t = getTree(loadedPage);
  const cache = new SACache();

  // Start with root as scope
  let visit = SwitchAccessPredicate.visit(t.root, cache);
  assertTrue(visit(t.root), 'Root should be visited in root tree');
  assertTrue(visit(t.upper1), 'Upper1 should be visited in root tree');
  assertTrue(visit(t.upper2), 'Upper2 should be visited in root tree');

  // Set upper1 as scope
  visit = SwitchAccessPredicate.visit(t.upper1, cache);
  assertTrue(visit(t.upper1), 'Upper1 should be visited in upper1 tree');
  assertTrue(visit(t.lower1), 'Lower1 should be visited in upper1 tree');
  assertFalse(visit(t.lower2), 'Lower2 should not be visited in upper1 tree');
  assertFalse(visit(t.leaf4), 'Leaf4 should not be visited in upper1 tree');
  assertTrue(visit(t.leaf5), 'Leaf5 should be visited in upper1 tree');

  // Set lower1 as scope
  visit = SwitchAccessPredicate.visit(t.lower1, cache);
  assertTrue(visit(t.lower1), 'Lower1 should be visited in lower1 tree');
  assertTrue(visit(t.leaf1), 'Leaf1 should be visited in lower1 tree');
  assertFalse(visit(t.leaf2), 'Leaf2 should not be visited in lower1 tree');
  assertTrue(visit(t.leaf3), 'Leaf3 should be visited in lower1 tree');

  // An uninteresting subtree should return false, regardless of scope
  assertFalse(visit(t.lower3), 'Lower3 should not be visited in lower1 tree');
  assertFalse(visit(t.leaf6), 'Leaf6 should not be visited in lower1 tree');
  assertFalse(visit(t.leaf7), 'Leaf7 should not be visited in lower1 tree');
});

AX_TEST_F('SwitchAccessPredicateTest', 'Cache', async function() {
  const loadedPage = await this.runWithLoadedTree(testWebsite());
  const t = getTree(loadedPage);
  const cache = new SACache();

  let locationAccessCount = 0;
  class TestRoot extends SARootNode {
    /** @override */
    get location() {
      locationAccessCount++;
      return null;
    }
  }
  const group = new TestRoot(t.root);

  assertTrue(
      SwitchAccessPredicate.isGroup(t.root, group, cache),
      'Root should be a group');
  assertEquals(
      locationAccessCount, 1,
      'Location should have been accessed to calculate isGroup');
  assertTrue(
      SwitchAccessPredicate.isGroup(t.root, group, cache),
      'isGroup value should not change');
  assertEquals(
      locationAccessCount, 1,
      'Cache should have been used, avoiding second location access');

  locationAccessCount = 0;
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf1, group, cache),
      'Leaf should not be a group');
  assertEquals(
      locationAccessCount, 1,
      'Location should have been accessed to calculate isGroup');
  assertFalse(
      SwitchAccessPredicate.isGroup(t.leaf1, group, cache),
      'isGroup value should not change');
  assertEquals(
      locationAccessCount, 1,
      'Cache should have been used, avoiding second location access');
});
