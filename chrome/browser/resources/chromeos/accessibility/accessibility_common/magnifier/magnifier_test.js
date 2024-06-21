// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);

/**
 * Magnifier feature using accessibility common extension browser tests.
 */
MagnifierE2ETest = class extends E2ETestBase {
  constructor() {
    super();
    window.RoleType = chrome.automation.RoleType;
  }

  async getNextMagnifierBounds() {
    return new Promise((resolve) => {
      const listener = (magnifierBounds) => {
        chrome.accessibilityPrivate.onMagnifierBoundsChanged.removeListener(
            listener);
        resolve(magnifierBounds);
      };
      chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
          listener);
    });
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "ui/accessibility/accessibility_features.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    base::OnceClosure load_cb =
        base::BindOnce(&ash::MagnificationManager::SetMagnifierEnabled,
            base::Unretained(ash::MagnificationManager::Get()),
            true);
      `);
    super.testGenPreambleCommon('kAccessibilityCommonExtensionId');
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kAccessibilityMagnifierFollowsChromeVox',
        'features::kAccessibilityMagnifierFollowsSts',
      ],
    };
  }
};

// Flaky: http://crbug.com/1171635
AX_TEST_F(
    'MagnifierE2ETest', 'DISABLED_MovesScreenMagnifierToFocusedElement',
    async function() {
      const site = `
        <button id="apple">Apple</button><br />
        <button id="banana" style="margin-top: 400px">Banana</button>
      `;
      const root = await this.runWithLoadedTree(site);
      const magnifier = accessibilityCommon.getMagnifierForTest();
      magnifier.setIsInitializingForTest(false);

      const apple = root.find({attributes: {name: 'Apple'}});
      const banana = root.find({attributes: {name: 'Banana'}});

      // Focus and move magnifier to apple.
      apple.focus();

      // Verify magnifier bounds contains apple, but not banana.
      let bounds = await this.getNextMagnifierBounds();
      assertTrue(RectUtil.contains(bounds, apple.location));
      assertFalse(RectUtil.contains(bounds, banana.location));

      // Focus and move magnifier to banana.
      banana.focus();

      // Verify magnifier bounds contains banana, but not apple.
      bounds = await this.getNextMagnifierBounds();
      assertFalse(RectUtil.contains(bounds, apple.location));
      assertTrue(RectUtil.contains(bounds, banana.location));
    });

// Disabled - flaky: https://crbug.com/1145612
AX_TEST_F(
    'MagnifierE2ETest', 'DISABLED_MovesDockedMagnifierToActiveDescendant',
    async function() {
      const site = `
    <div role="group" id="parent" aria-activedescendant="apple">
      <div id="apple" role="treeitem">Apple</div>
      <div id="banana" role="treeitem">Banana</div>
    </div>
    <script>
      const parent = document.getElementById('parent');
      parent.addEventListener('click', function() {
        parent.setAttribute('aria-activedescendant', 'banana');
      });
      </script>
  `;
      const root = await this.runWithLoadedTree(site);
      // Enable docked magnifier.
      await new Promise((resolve) => {
        chrome.accessibilityFeatures.dockedMagnifier.set(
            {value: true}, resolve);
      });

      // Validate magnifier wants to move to root.
      const rootLocation = await getNextMagnifierLocation();
      assertTrue(RectUtil.equal(rootLocation, root.location));

      // Click parent to change active descendant from apple to banana.
      const parent = root.find({role: RoleType.GROUP});
      parent.doDefault();

      // Register and wait for rect from magnifier.
      const rect = await getNextMagnifierLocation();

      // Validate rect from magnifier is rect of banana.
      const bananaNode = root.find({
        role: RoleType.TREE_ITEM,
        attributes: {name: 'Banana'},
      });
      assertTrue(RectUtil.equal(rect, bananaNode.location));
    });

// Flaky: http://crbug.com/1171750
AX_TEST_F(
    'MagnifierE2ETest', 'DISABLED_MovesScreenMagnifierToActiveDescendant',
    async function() {
      const site = `
    <span tabindex="1">Top</span>
    <div id="group" role="group" style="width: 200px"
        aria-activedescendant="apple">
      <div id="apple" role="treeitem">Apple</div>
      <div id="banana" role="treeitem" style="margin-top: 400px">Banana</div>
    </div>
    <script>
      const group = document.getElementById('group');
      group.addEventListener('click', function() {
        group.setAttribute('aria-activedescendant', 'banana');
      });
    </script>
  `;
      const root = await this.runWithLoadedTree(site);
      const magnifier = accessibilityCommon.getMagnifierForTest();
      magnifier.setIsInitializingForTest(false);

      const top = root.find({attributes: {name: 'Top'}});
      const banana = root.find({attributes: {name: 'Banana'}});
      const group = root.find({role: RoleType.GROUP});

      // Focus and move magnifier to top.
      top.focus();

      // Verify magnifier bounds don't contain banana.
      let bounds = await this.getNextMagnifierBounds();
      assertFalse(RectUtil.contains(bounds, banana.location));

      // Click group to change active descendant to banana.
      group.doDefault();

      // Verify magnifier bounds contain banana.
      bounds = await this.getNextMagnifierBounds();
      assertTrue(RectUtil.contains(bounds, banana.location));
    });

TEST_F(
    'MagnifierE2ETest', 'MovesFullscreenMagnifierSelectionEvent', function() {
      this.runWithLoadedDesktop(async function(desktop) {
        const magnifier = accessibilityCommon.getMagnifierForTest();
        magnifier.setIsInitializingForTest(false);

        const moveMenuSelectionAssertBounds = async (targetBounds) => {
          // Send arrow up key.
          chrome.accessibilityPrivate.sendSyntheticKeyEvent({
            type:
                chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
            keyCode: KeyCode.UP,
          });

          // Verify new magnifier bounds include |targetBounds|.
          await new Promise((resolve) => {
            const boundsChangedListener = (newBounds) => {
              if (RectUtil.contains(newBounds, targetBounds)) {
                chrome.accessibilityPrivate.onMagnifierBoundsChanged
                    .removeListener(boundsChangedListener);
                resolve();
              }
            };
            chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
                boundsChangedListener);
          });
        };

        // Trigger Chrome menu.
        chrome.accessibilityPrivate.sendSyntheticKeyEvent({
          type: chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
          keyCode: KeyCode.E,
          modifiers: {alt: true},
        });

        // Wait for Chrome menu to open.
        await new Promise(
            (resolve) => desktop.addEventListener(
                chrome.automation.EventType.MENU_START, resolve, false));

        // Move menu selection to end of menu, and await new magnifier bounds.
        await moveMenuSelectionAssertBounds({
          left: 650,
          top: 450,
          width: 0,
          height: 0,
        });
      });
    });

AX_TEST_F('MagnifierE2ETest', 'IgnoresRootNodeFocus', async function() {
  await this.runWithLoadedTree('');

  const magnifier = accessibilityCommon.getMagnifierForTest();
  magnifier.setIsInitializingForTest(false);

  chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
      (newBounds) => {
        throw new Error(
            'Magnifier did not ignore focus change on document load - ' +
            'moved to following location: ' + JSON.stringify(newBounds));
      });

  // Wait seven seconds to verify magnifier successfully ignored focus on root
  // node.
  await new Promise((resolve) => setTimeout(resolve, 7000));
});

// TODO(crbug.com/1295685): Test is flaky.
AX_TEST_F(
    'MagnifierE2ETest', 'DISABLED_MagnifierCenterOnPoint', async function() {
      await this.runWithLoadedTree('');
      const magnifier = accessibilityCommon.getMagnifierForTest();
      magnifier.setIsInitializingForTest(false);

      const movePointAssertBounds = async (targetPoint, targetBounds) => {
        // Repeatedly center magnifier on |targetPoint|.
        const id = setInterval(() => {
          chrome.accessibilityPrivate.magnifierCenterOnPoint(targetPoint);
        }, 500);

        // Verify new magnifier bounds include |targetBounds|.
        await new Promise((resolve) => {
          const boundsChangedListener = (newBounds) => {
            if (RectUtil.contains(newBounds, targetBounds)) {
              chrome.accessibilityPrivate.onMagnifierBoundsChanged
                  .removeListener(boundsChangedListener);
              clearInterval(id);
              resolve();
            }
          };
          chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
              boundsChangedListener);
        });
      };

      // Move magnifier to upper left of screen.
      await movePointAssertBounds(
          {x: 100, y: 100}, {left: 100, top: 100, width: 0, height: 0});

      // Move magnifier to lower right of screen.
      await movePointAssertBounds(
          {x: 650, y: 450}, {left: 650, top: 450, width: 0, height: 0});
    });

AX_TEST_F('MagnifierE2ETest', 'OnCaretBoundsChanged', async function() {
  const site = `
    <input type="text" id="input" style="width: 1000px">
    <button id="button">Type words</button>
    <script>
      const input = document.getElementById('input');
      const button = document.getElementById('button');
      button.addEventListener('click', function() {
        input.focus();
        input.value += 'The quick brown fox jumps over the lazy dog.';
      });
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  const magnifier = accessibilityCommon.getMagnifierForTest();
  magnifier.setIsInitializingForTest(false);
  const button = root.find({attributes: {name: 'Type words'}});
  const input = root.find({role: RoleType.TEXT_FIELD});
  input.doDefault();

  const typeWordsAssertBounds = async (targetBounds) => {
    // Type words in the input field to move the text caret forward.
    const id = setInterval(() => {
      button.doDefault();
    }, 500);

    // Verify new magnifier bounds include |targetBounds|.
    await new Promise((resolve) => {
      const boundsChangedListener = (newBounds) => {
        if (RectUtil.contains(newBounds, targetBounds)) {
          chrome.accessibilityPrivate.onMagnifierBoundsChanged.removeListener(
              boundsChangedListener);
          clearInterval(id);
          resolve();
        }
      };
      chrome.accessibilityPrivate.onMagnifierBoundsChanged.addListener(
          boundsChangedListener);
    });
  };

  // Type words to move text cursor forward, verify magnifier contains caret.
  await typeWordsAssertBounds({left: 400, top: 100, width: 0, height: 0});

  // Additional words to move caret forward, make sure magnifier follows.
  await typeWordsAssertBounds({left: 800, top: 100, width: 0, height: 0});

  // Even more words to move caret forward, make sure magnifier follows.
  await typeWordsAssertBounds({left: 1200, top: 100, width: 0, height: 0});
});

TEST_F('MagnifierE2ETest', 'ScreenMagnifierFocusFollowingPref', function() {
  this.newCallback(async () => {
    // Disable focus following for full screen magnifier, and verify prefs and
    // state.
    await this.setPref(Magnifier.Prefs.SCREEN_MAGNIFIER_FOCUS_FOLLOWING, false);
    magnifier = accessibilityCommon.getMagnifierForTest();
    magnifier.setIsInitializingForTest(false);
    assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
    assertFalse(magnifier.shouldFollowFocus());

    // Enable focus following for full screen magnifier, and verify prefs and
    // state.
    await this.setPref(Magnifier.Prefs.SCREEN_MAGNIFIER_FOCUS_FOLLOWING, true);
    magnifier = accessibilityCommon.getMagnifierForTest();
    magnifier.setIsInitializingForTest(false);
    assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
    assertTrue(magnifier.shouldFollowFocus());
  })();
});

TEST_F(
    'MagnifierE2ETest', 'ScreenMagnifierSelectToSpeakFollowingPref',
    function() {
      this.newCallback(async () => {
        // Disable select to speak following for full screen magnifier, and
        // verify prefs and state.
        await this.setPref(
            Magnifier.Prefs.SCREEN_MAGNIFIER_SELECT_TO_SPEAK_FOCUS_FOLLOWING,
            false);
        magnifier = accessibilityCommon.getMagnifierForTest();
        magnifier.setIsInitializingForTest(false);
        assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
        assertFalse(magnifier.shouldFollowStsFocus());

        // Enable select to speak following for full screen magnifier, and
        // verify prefs and state.
        await this.setPref(
            Magnifier.Prefs.SCREEN_MAGNIFIER_SELECT_TO_SPEAK_FOCUS_FOLLOWING,
            true);
        magnifier = accessibilityCommon.getMagnifierForTest();
        magnifier.setIsInitializingForTest(false);
        assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
        assertTrue(magnifier.shouldFollowStsFocus());
      })();
    });

TEST_F(
    'MagnifierE2ETest', 'FullscreenMagnifierDoesNotFollowStsWhenPrefOff',
    function() {
      this.newCallback(async () => {
        // Disable select to speak following for full screen magnifier, and
        // verify prefs and state.
        await this.setPref(
            Magnifier.Prefs.SCREEN_MAGNIFIER_SELECT_TO_SPEAK_FOCUS_FOLLOWING,
            false);
        magnifier = accessibilityCommon.getMagnifierForTest();
        magnifier.setIsInitializingForTest(false);
        assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
        assertFalse(magnifier.shouldFollowStsFocus());

        let count = 0;
        chrome.accessibilityPrivate.moveMagnifierToRect = () => (count += 1);
        assertEquals(count, 0);
        magnifier.onSelectToSpeakFocusChanged_({
          left: 2,
          top: 4,
          width: 5,
          height: 7,
        });
        assertEquals(count, 0);
      })();
    });

TEST_F(
    'MagnifierE2ETest', 'FullscreenMagnifierFollowsStsWhenPrefOn', function() {
      this.newCallback(async () => {
        // Disable select to speak following for full screen magnifier, and
        // verify prefs and state.
        await this.setPref(
            Magnifier.Prefs.SCREEN_MAGNIFIER_SELECT_TO_SPEAK_FOCUS_FOLLOWING,
            true);
        magnifier = accessibilityCommon.getMagnifierForTest();
        magnifier.setIsInitializingForTest(false);
        assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
        assertTrue(magnifier.shouldFollowStsFocus());

        let count = 0;
        chrome.accessibilityPrivate.moveMagnifierToRect = () => (count += 1);
        magnifier.lastMouseMovedTime_ = undefined;
        assertEquals(count, 0);
        magnifier.onSelectToSpeakFocusChanged_({
          left: 2,
          top: 4,
          width: 5,
          height: 7,
        });
        assertEquals(count, 1);
      })();
    });

TEST_F('MagnifierE2ETest', 'ScreenMagnifierChromeVoxFollowingPref', function() {
  this.newCallback(async () => {
    // Disable ChromeVox following for full screen magnifier, and
    // verify prefs and state.
    await this.setPref(
        Magnifier.Prefs.SCREEN_MAGNIFIER_CHROMEVOX_FOCUS_FOLLOWING, false);
    magnifier = accessibilityCommon.getMagnifierForTest();
    magnifier.setIsInitializingForTest(false);
    assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
    assertFalse(magnifier.shouldFollowChromeVoxFocus());

    // Enable ChromeVox following for full screen magnifier, and
    // verify prefs and state.
    await this.setPref(
        Magnifier.Prefs.SCREEN_MAGNIFIER_CHROMEVOX_FOCUS_FOLLOWING, true);
    magnifier = accessibilityCommon.getMagnifierForTest();
    magnifier.setIsInitializingForTest(false);
    assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
    assertTrue(magnifier.shouldFollowChromeVoxFocus());
  })();
});

TEST_F(
    'MagnifierE2ETest', 'ScreenMagnifierChromeVoxDoesNotFollowWhenPrefOff',
    function() {
      this.newCallback(async () => {
        // Disable ChromeVox following for full screen magnifier, and
        // verify prefs and state.
        await this.setPref(
            Magnifier.Prefs.SCREEN_MAGNIFIER_CHROMEVOX_FOCUS_FOLLOWING, false);
        magnifier = accessibilityCommon.getMagnifierForTest();
        magnifier.setIsInitializingForTest(false);
        assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
        assertFalse(magnifier.shouldFollowChromeVoxFocus());

        let count = 0;
        chrome.accessibilityPrivate.moveMagnifierToRect = () => (count += 1);
        assertEquals(count, 0);
        magnifier.onChromeVoxFocusChanged_({
          left: 2,
          top: 4,
          width: 5,
          height: 7,
        });
        assertEquals(count, 0);
      })();
    });

TEST_F(
    'MagnifierE2ETest', 'ScreenMagnifierChromeVoxFollowsWhenPrefOn',
    function() {
      this.newCallback(async () => {
        // Disable ChromeVox following for full screen magnifier, and
        // verify prefs and state.
        await this.setPref(
            Magnifier.Prefs.SCREEN_MAGNIFIER_CHROMEVOX_FOCUS_FOLLOWING, true);
        magnifier = accessibilityCommon.getMagnifierForTest();
        magnifier.setIsInitializingForTest(false);
        assertEquals(magnifier.type, Magnifier.Type.FULL_SCREEN);
        assertTrue(magnifier.shouldFollowChromeVoxFocus());

        let count = 0;
        chrome.accessibilityPrivate.moveMagnifierToRect = () => (count += 1);
        magnifier.lastMouseMovedTime_ = undefined;
        assertEquals(count, 0);
        magnifier.onChromeVoxFocusChanged_({
          left: 2,
          top: 4,
          width: 5,
          height: 7,
        });
        assertEquals(count, 1);
      })();
    });
