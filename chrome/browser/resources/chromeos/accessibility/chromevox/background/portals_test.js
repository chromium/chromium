// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for Portals.
 */
ChromeVoxPortalsTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    window.EventType = chrome.automation.EventType;
    window.RoleType = chrome.automation.RoleType;
    window.doCmd = this.doCmd;
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`#include "third_party/blink/public/common/features.h"`);
  }

  /** @override */
  get featureList() {
    return {enabled: ['blink::features::kPortals']};
  }

  get testServer() {
    return true;
  }

  /**
   * Waits for |portal|'s tree to be ready.
   * @param {chrome.automation.AutomationNode} portal
   * @return {Promise}
   */
  async waitForPortal(portal) {
    const waitForChildren = () => new Promise(r => {
      const hasChildren = () => portal.children.length > 0;
      if (hasChildren()) {
        r();
        return;
      }
      const onChildrenChanged = () => {
        portal.removeEventListener(
            EventType.CHILDREN_CHANGED, onChildrenChanged, true);
        r();
      };
      portal.addEventListener(
          EventType.CHILDREN_CHANGED, onChildrenChanged, true);
    });

    const waitForLoaded = () => new Promise(r => {
      const hasLoaded = () => portal.children[0].docLoaded;
      if (hasLoaded()) {
        r();
        return;
      }
      const onLoadComplete = () => {
        portal.removeEventListener(
            EventType.LOAD_COMPLETE, onLoadComplete, true);
        r();
      };
      portal.addEventListener(EventType.LOAD_COMPLETE, onLoadComplete, true);
    });

    await waitForChildren();
    await waitForLoaded();
  }
};

TEST_F('ChromeVoxPortalsTest', 'ShouldFocusPortal', function() {
  this.runWithLoadedTree(
      undefined, function(root) {
        const portal = root.find({role: RoleType.PORTAL});
        const button = root.find({role: RoleType.BUTTON});
        assertEquals(RoleType.PORTAL, portal.role);
        assertEquals(RoleType.BUTTON, button.role);

        const afterPortalIsReady = this.newCallback(() => {
          const chromeVoxState = ChromeVoxState.instance;
          portal.addEventListener(EventType.FOCUS, this.newCallback(function() {
            assertEquals(portal, chromeVoxState.currentRange.start.node);
            // test is done.
          }));
          assertEquals(button, chromeVoxState.currentRange.start.node);
          doCmd('nextObject')();
        });

        button.focus();
        button.addEventListener(
            EventType.FOCUS,
            () => this.waitForPortal(portal).then(afterPortalIsReady));
      }.bind(this), {
        url:
            `${testRunnerParams.testServerBaseUrl}portal/portal-and-button.html`
      });
});

TEST_F('ChromeVoxPortalsTest', 'PortalName', function() {
  this.runWithLoadedTree(
      undefined, function(root) {
        const portal = root.find({role: RoleType.PORTAL});
        assertEquals(RoleType.PORTAL, portal.role);
        this.waitForPortal(portal).then(this.newCallback(() => {
          assertTrue(portal.firstChild.docLoaded);
          assertEquals(portal.name, 'some text');
        }));
      }.bind(this), {
        url: `${testRunnerParams.testServerBaseUrl}portal/portal-with-text.html`
      });
});
