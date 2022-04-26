// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../../../common/testing/documents.js',
  '../../testing/chromevox_next_e2e_test_base.js',
]);

// Fake Msgs object.
const Msgs = {
  getMsg: () => 'None'
};

// Fake PanelBridge.
const PanelBridge = {
  addPanelNodeMenuItems: (id, items) => PanelBridge.calls.append({id, items}),
  calls: [],
};

/** Test fixture for PanelNodeMenuBackground. */
ChromeVoxPanelNodeMenuBackgroundTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'PanelNodeMenuBackground',
        '/chromevox/background/panel/panel_node_menu_background.js');
    this.expectedMenuCount = PanelNodeMenuBackground.roleListMenuMapping.length;
  }

  // Yield so populateAll_ has a chance to run.
  yieldForPopulation() {
    return new Promise(resolve => setTimeout(resolve, 0));
  }

  get headingsDoc() {
    return `<h1>Heading 1</h1>
        <h2>Heading 2</h2>
        <h3>Heading 3</h3>`;
  }

  get landmarksDoc() {
    return [
        Documents.application,
        Documents.banner,
        Documents.complementary,
        Documents.form,
        Documents.main,
        Documents.navigation,
        Documents.region,
        Documents.search
    ].join('\n');
  }

  get linksDoc() {
    return `<a href="#a">Link 1</a>
        <a href="#b">Link 2</a>
        <a href="#c">Link 3</a>
        <a href="#d">Link 4</a>
        <p>Not a link</p>`;
  }

  get formControlsDoc() {
    return [
        Documents.button,
        Documents.textInput,
        Documents.textarea,
        `<p>Static text</p>`,
        Documents.checkbox,
        Documents.color,
        Documents.slider,
        Documents.switch,
        Documents.tab,
        Documents.tree,
        `<script>
          document.getElementById('tree').focus();
        </script>`
    ].join('\n');
  }

  get mixedDoc() {
    return [
        Documents.button,
        Documents.table,
        Documents.region,
        Documents.link,
        Documents.header
    ].join('\n');
  }

  get tablesDoc() {
    return [
        Documents.grid,
        Documents.table
    ].join('\n');
  }
};

TEST_F(
    'ChromeVoxPanelNodeMenuBackgroundTest', 'EmptyDocument', async function() {
      await this.runWithLoadedTree('');
      const menus = PanelNodeMenuBackground.createAllPanelNodeMenuData();
      assertEquals(this.expectedMenuCount, menus.length);

      // Verify that the menus are created in the expected order.
      assertEquals('role_heading', menus[0].title);
      assertEquals('role_landmark', menus[1].title);
      assertEquals('role_link', menus[2].title);
      assertEquals('panel_menu_form_controls', menus[3].title);
      assertEquals('role_table', menus[4].title);

      // Verify that menu ids are unique.
      for (const menu of menus) {
        for (const otherMenu of menus) {
          if (menu !== otherMenu) {
            assertNotEquals(menu.menuId, otherMenu.menuId);
          }
        }
      }

      await this.yieldForPopulation();

      // With an empty document, we expect one call to addPanelNodeMenuItems per
      // menu.
      assertEquals(this.expectedMenuCount, PanelBridge.calls.length);

      // We expect the calls to add menu items indicating there are no relevant
      // nodes.
      for (const call of PanelBridge.calls) {
        assertEquals(1, call.items.length);
        assertEquals('None', call.items[0].title);
        assertEquals(-1, call.items[0].callbackId);
        assertFalse(call.items[0].isActive);
      }
    });

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Headings', async function() {
  await this.runWithLoadedTree(this.headingsDoc);
  const menus = PanelNodeMenuBackground.createAllPanelNodeMenuData();

  assertEquals(this.expectedMenuCount, menus.length);
  assertEquals('role_heading', menus[0].title);
  const headingMenuId = menus[0].menuId;

  await this.yieldForPopulation();

  // The heading menu should have less than 5 items, which is the batch size, so
  // we expect only one call for each menu (the others all being empty).
  assertEquals(this.expectedMenuCount, PanelBridge.calls.length);

  let headingCall;
  // All calls to add menu items other than the heading menu call should
  // indicate there are no relevant nodes.
  for (const call of PanelBridge.calls) {
    if (call.id === headingMenuId) {
      headingCall = call;
      continue;
    }
    assertEquals(1, call.items.length);
    assertEquals('None', call.items[0].title);
  }

  // Check the contents of the heading call match what we expect based on the
  // provided document.
  const headingMenuItems = headingCall.items;
  assertEquals(3, headingMenuItems.length);
  assertEquals('Heading 1', headingMenuItems[0].title);
  assertEquals('Heading 2', headingMenuItems[1].title);
  assertEquals('Heading 3', headingMenuItems[2].title);

  for (const item of headingMenuItems) {
    assertFalse(item.isActive);
    // Verify the callback IDs are unique.
    for (const otherItem of headingMenuItems) {
      if (item !== otherItem) {
        assertNotEquals(item.callbackId, otherItem.callbackId);
      }
    }
  }
});

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Landmarks', async function() {
  await this.runWithLoadedTree(this.landmarksDoc);
  const menus = PanelNodeMenuBackground.createAllPanelNodeMenuData();

  assertEquals(this.expectedMenuCount, menus.length);
  assertEquals('role_landmark', menus[1].title);
  const landmarkMenuId = menus[1].menuId;

  await this.yieldForPopulation();

  // The landmarks menu will have a total of 2 calls, as it has more than 5
  // items (the batch size). However, only one batch of landmarks occurs per
  // call to yieldForPopulation, so expect one call per menu (the others all
  // being empty).
  assertEquals(this.expectedMenuCount, PanelBridge.calls.length);

  let landmarkCall;
  // All calls to add menu items other than the landmark menu call should
  // indicate there are no relevant nodes.
  for (const call of PanelBridge.calls) {
    if (call.id === landmarkMenuId) {
      landmarkCall = call;
      continue;
    }
    assertEquals(1, call.items.length);
    assertEquals('None', call.items[0].title);
  }

  // Check the contents of the landmark call match what we expect based on the
  // provided document.
  const landmarkMenuItems = landmarkCall.items;
  assertEquals(5, landmarkMenuItems.length);

  // Wait for the rest of the menu items to populate.
  PanelBridge.calls = [];
  await this.yieldForPopulation();
  assertEquals(1, PanelBridge.calls.length);

  assertEquals(landmarkMenuId, PanelBridge.calls[0].id);
  landmarkMenuItems.append(PanelBridge.calls[0].items);
  assertEquals(8, landmarkMenuItems.length);

  assertEquals('application', landmarkMenuItems[0].title);
  assertEquals('banner', landmarkMenuItems[1].title);
  assertEquals('complementary', landmarkMenuItems[2].title);
  assertEquals('form', landmarkMenuItems[3].title);
  assertEquals('main', landmarkMenuItems[4].title);
  assertEquals('navigation', landmarkMenuItems[5].title);
  assertEquals('region', landmarkMenuItems[6].title);
  assertEquals('search', landmarkMenuItems[7].title);

  for (const item of landmarkMenuItems) {
    assertFalse(item.isActive);
    // Verify the callback IDs are unique.
    for (const otherItem of landmarkMenuItems) {
      if (item !== otherItem) {
        assertNotEquals(item.callbackId, otherItem.callbackId);
      }
    }
  }
});

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Links', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  const menus = PanelNodeMenuBackground.createAllPanelNodeMenuData();

  assertEquals(this.expectedMenuCount, menus.length);
  assertEquals('role_link', menus[2].title);
  const linkMenuId = menus[2].menuId;

  await this.yieldForPopulation();

  // The link menu should have less than 5 items, which is the batch size, so
  // we expect only one call for each menu (the others all being empty).
  assertEquals(this.expectedMenuCount, PanelBridge.calls.length);

  let linkCall;
  // All calls to add menu items other than the link menu call should
  // indicate there are no relevant nodes.
  for (const call of PanelBridge.calls) {
    if (call.id === linkMenuId) {
      linkCall = call;
      continue;
    }
    assertEquals(1, call.items.length);
    assertEquals('None', call.items.title);
  }

  // Check the contents of the link call match what we expect based on the
  // provided document.
  const linkMenuItems = linkCall.items;
  assertEquals(4, linkMenuItems.length);
  assertEquals('Link 1', linkMenuItems[0].title);
  assertEquals('Link 2', linkMenuItems[1].title);
  assertEquals('Link 3', linkMenuItems[2].title);
  assertEquals('Link 4', linkMenuItems[3].title);

  for (const item of linkMenuItems) {
    assertFalse(item.isActive);
    // Verify the callback IDs are unique.
    for (const otherItem of linkMenuItems) {
      if (item !== otherItem) {
        assertNotEquals(item.callbackId, otherItem.callbackId);
      }
    }
  }
});

TEST_F(
    'ChromeVoxPanelNodeMenuBackgroundTest', 'FormControls', async function() {
      await this.runWithLoadedTree(this.formControlsDoc);
      const menus = PanelNodeMenuBackground.createAllPanelNodeMenuData(
          'panel_menu_form_controls');

      assertEquals(this.expectedMenuCount, menus.length);
      assertEquals('panel_menu_form_controls', menus[3].title);
      const formMenuId = menus[3].menuId;

      // The form controls menu will have a total of 2 calls, as it has more
      // than 5 items (the batch size). And because it is specified as the
      // activated menu, we expect both of those calls to happen before any
      // others.
      assertEquals(this.expectedMenuCount + 1, PanelBridge.calls.length);
      const formCall1 = PanelBridge.calls.shift();
      assertEquals(formMenuId, formCall1.id);
      const formCall2 = PanelBridge.calls.shift();
      assertEquals(formMenuId, formCall2.id);

      // All calls to add menu items other than the form control menu call
      // should indicate there are no relevant nodes.
      for (const call of PanelBridge.calls) {
        assertNotEquals(formMenuId, call.id);
        assertEquals(1, call.items.length);
        assertEquals('None', call.items[0].title);
      }

      // Check the contents of the form control call match what we expect based
      // on the provided document.
      const formMenuItems = formCall1.items;
      formMenuItems.append(formCall2.items);
      assertEquals(9, formMenuItems.length);

      assertEquals('button', formMenuItems[0].title);
      assertEquals('textInput', formMenuItems[1].title);
      assertEquals('textarea', formMenuItems[2].title);
      assertEquals('checkbox', formMenuItems[3].title);
      assertEquals('color', formMenuItems[4].title);
      assertEquals('slider', formMenuItems[5].title);
      assertEquals('switch', formMenuItems[6].title);
      assertEquals('tab', formMenuItems[7].title);
      assertEquals('tree', formMenuItems[8].title);

      // Because this menu is activated, and the focus is on the tree, we expect
      // it to be active as well.
      const treeItem = formMenuItems.pop();
      assertTrue(treeItem.isActive);

      for (const item of formMenuItems) {
        assertFalse(item.isActive);
      }
    });

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Tables', async function() {
  await this.runWithLoadedTree(this.tablesDoc);
  const menus = PanelNodeMenuBackground.createAllPanelNodeMenuData();

  assertEquals(this.expectedMenuCount, menus.length);
  assertEquals('role_table', menus[4].title);
  const tableMenuId = menus[4].menuId;

  await this.yieldForPopulation();

  // The table menu should have less than 5 items, which is the batch size, so
  // we expect only one call for each menu (the others all being empty).
  assertEquals(this.expectedMenuCount, PanelBridge.calls.length);

  let tableCall;
  // All calls to add menu items other than the table menu call should
  // indicate there are no relevant nodes.
  for (const call of PanelBridge.calls) {
    if (call.id === tableMenuId) {
      tableCall = call;
      continue;
    }
    assertEquals(1, call.items.length);
    assertEquals('None', call.items.title);
  }

  // Check the contents of the table call match what we expect based on the
  // provided document.
  const tableMenuItems = tableCall.items;
  assertEquals(2, tableMenuItems.length);
  assertEquals('grid', tableMenuItems[0].title);
  assertEquals('table', tableMenuItems[1].title);

  assertFalse(tableMenuItems[0].isActive);
  assertFalse(tableMenuItems[1].isActive);
  assertNotEquals(tableMenuItems[0].callbackId, tableMenuItems[1].callbackId);
});

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'MixedData', async function() {
  await this.runWithLoadedTree(this.mixedDoc);
  const menus = PanelNodeMenuBackground.createAllPanelNodeMenuData();

  assertEquals('role_heading', menus[0].title);
  const headingId = menus[0].menuId;
  assertEquals('role_landmark', menus[1].title);
  const landmarkId = menus[1].menuId;
  assertEquals('role_link', menus[2].title);
  const linkId = menus[2].menuId;
  assertEquals('panel_menu_form_controls', menus[3].title);
  const formId = menus[3].menuId;
  assertEquals('role_table', menus[4].title);
  const tableId = menus[4].menuId;

  await this.yieldForPopulation();
  assertEquals(this.expectedMenuCount, PanelBridge.calls.length);

  const headingCall = PanelBridge.calls.find(call => call.id === headingId);
  const landmarkCall = PanelBridge.calls.find(call => call.id === landmarkId);
  const linkCall = PanelBridge.calls.find(call => call.id === linkId);
  const formCall = PanelBridge.calls.find(call => call.id === formId);
  const tableCall = PanelBridge.calls.find(call => call.id === tableId);

  assertEquals(1, headingCall.items.length);
  assertEquals('header', headingCall.items[0].title);
  assertEquals(1, landmarkCall.items.length);
  assertEquals('region', landmarkCall.items[0].title);
  assertEquals(1, linkCall.items.length);
  assertEquals('link', linkCall.items[0].title);
  assertEquals(1, formCall.items.length);
  assertEquals('button', formCall.items[0].title);
  assertEquals(1, tableCall.items.length);
  assertEquals('table', tableCall.items[0].title);
});
