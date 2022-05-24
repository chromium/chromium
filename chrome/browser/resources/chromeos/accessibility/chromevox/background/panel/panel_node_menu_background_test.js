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
  addMenuItem: (item, id) => PanelBridge.calls.append({item, id}),
  calls: [],
};

/** Test fixture for PanelNodeMenuBackground. */
ChromeVoxPanelNodeMenuBackgroundTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'PanelBackground', '/chromevox/background/panel/panel_background.js');
    await importModule(
        'PanelNodeMenuBackground',
        '/chromevox/background/panel/panel_node_menu_background.js');
  }

  assertMenuItemIndicatesNoNodesFound(item) {
    assertNotNullNorUndefined(item);
    assertEquals('None', item.title);
    assertEquals(-1, item.callbackId);
    assertFalse(item.isActive);
  }

  assertItemMatches(expectedName, item, opt_isActive) {
    assertEquals(expectedName, item.title);
    assertTrue(
        item.callbackId >= 0 &&
        item.callbackId < PanelNodeMenuBackground.callbackNodes_.length);
    assertEquals(
        expectedName,
        PanelNodeMenuBackground.callbackNodes_[item.callbackId].name);
    if (opt_isActive) {
      assertTrue(item.isActive);
    } else {
      assertFalse(item.isActive);
    }
  }

  assertMenusHaveNoNodesFoundExcept(id) {
    for (const menu of PanelNodeMenuData.ALL_NODE_MENUS) {
      if (menu.menuId === id) {
        continue;
      }
      const call = PanelBridge.calls.find(args => args.id === menu.menuId);
      assertNotNullNorUndefined(call);
      this.assertMenuItemIndicatesNoNodesFound(call.item);
    }
  }

  createAllNodeMenuBackgrounds(opt_activateMenu) {
    PanelBridge.calls = [];
    PanelBackground.instance.createAllNodeMenuBackgrounds_();
  }

  isFormControl(args) {
    return args.id === PanelNodeMenuId.FORM_CONTROL;
  }

  isHeading(args) {
    return args.id === PanelNodeMenuId.HEADING;
  }

  isLandmark(args) {
    return args.id === PanelNodeMenuId.LANDMARK;
  }

  isLink(args) {
    return args.id === PanelNodeMenuId.LINK;
  }

  isTable(args) {
    return args.id === PanelNodeMenuId.TABLE;
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
      this.createAllNodeMenuBackgrounds();

      // Expect that one element is added per menu, specifying that no nodes
      // of that type are found.
      assertEquals(
          PanelNodeMenuData.ALL_NODE_MENUS.length, PanelBridge.calls.length);
      // Assert all menus have a no nodes found element.
      this.assertMenusHaveNoNodesFoundExcept(null);
    });

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Headings', async function() {
  await this.runWithLoadedTree(this.headingsDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu, plus
  // two extra for the additional headings found).
  assertEquals(
      PanelNodeMenuData.ALL_NODE_MENUS_.length + 2, PanelBridge.calls.length);

  // Expect that the three items are added to the headings menu
  const headingItems =
      PanelBridge.calls.findAll(this.isHeading).map(args => args.item);
  assertEquals(3, headingItems.length);

  this.assertItemMatches('Heading 1', headingItems.unshift());
  this.assertItemMatches('Heading 2', headingItems.unshift());
  this.assertItemMatches('Heading 3', headingItems.unshift());

  this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.HEADING);
});

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Landmarks', async function() {
  await this.runWithLoadedTree(this.landmarksDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu, plus
  // seven extra for the additional landmarks found).
  assertEquals(
      PanelNodeMenuData.ALL_NODE_MENUS_.length + 7, PanelBridge.calls.length);

  // Verify that eight items were added to the landmarks menu.
  const landmarkItems =
      PanelBridge.calls.findAll(this.isLandmark).map(args => args.item);
  assertEquals(8, landmarkItems.length);

  this.assertItemMatches('application', landmarkItems.unshift());
  this.assertItemMatches('banner', landmarkItems.unshift());
  this.assertItemMatches('complementary', landmarkItems.unshift());
  this.assertItemMatches('form', landmarkItems.unshift());
  this.assertItemMatches('main', landmarkItems.unshift());
  this.assertItemMatches('navigation', landmarkItems.unshift());
  this.assertItemMatches('region', landmarkItems.unshift());
  this.assertItemMatches('search', landmarkItems.unshift());

  this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.LANDMARK);
});

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Links', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu, plus
  // three extra for the additional links found).
  assertEquals(
      PanelNodeMenuData.ALL_NODE_MENUS_.length + 3, PanelBridge.calls.length);

  // Verify that four items were added to the links menu.
  const linkItems =
      PanelBridge.calls.findAll(this.isLink).map(args => args.item);
  assertEquals(4, linkItems.length);

  this.assertItemMatches('Link 1', linkItems.unshift());
  this.assertItemMatches('Link 2', linkItems.unshift());
  this.assertItemMatches('Link 3', linkItems.unshift());
  this.assertItemMatches('Link 4', linkItems.unshift());

  this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.LINK);
});

TEST_F(
    'ChromeVoxPanelNodeMenuBackgroundTest', 'FormControls', async function() {
      await this.runWithLoadedTree(this.formControlsDoc);
      this.createAllNodeMenuBackgrounds('panel_menu_form_controls');

      // Check that there are the correct number of calls (one for each menu,
      // plus eight extra for the additional form controls found).
      assertEquals(
          PanelNodeMenuData.ALL_NODE_MENUS_.length + 8,
          PanelBridge.calls.length);

      // Verify that nine items were added to the form controls menu.
      const formItems =
          PanelBridge.calls.findAll(this.isFormControl).map(args => args.item);
      assertEquals(9, formItems.length);

      this.assertItemMatches('button', formItems.unshift());
      this.assertItemMatches('textInput', formItems.unshift());
      this.assertItemMatches('textarea', formItems.unshift());
      this.assertItemMatches('checkbox', formItems.unshift());
      this.assertItemMatches('color', formItems.unshift());
      this.assertItemMatches('slider', formItems.unshift());
      this.assertItemMatches('switch', formItems.unshift());
      this.assertItemMatches('tab', formItems.unshift());
      this.assertItemMatches('tree', formItems.unshift(), /* isActive= */ true);

      this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.FORM_CONTROL);
    });

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'Tables', async function() {
  await this.runWithLoadedTree(this.tablesDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu, plus
  // one extra for the additional links found).
  assertEquals(
      PanelNodeMenuData.ALL_NODE_MENUS_.length + 1, PanelBridge.calls.length);

  // Verify that two items were added to the tables menu.
  const tableItems =
      PanelBridge.calls.findAll(this.isTable).map(args => args.item);
  assertEquals(2, tableItems.length);

  this.assertItemMatches('grid', tableItems.unshift());
  this.assertItemMatches('table', tableItems.unshift());

  this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.TABLE);
});

TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'MixedData', async function() {
  await this.runWithLoadedTree(this.mixedDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu).
  assertEquals(
      PanelNodeMenuData.ALL_NODE_MENUS_.length, PanelBridge.calls.length);

  // Check that each item was added to the correct menu.
  const formItem = PanelBridge.calls.find(this.isFormControl).item;
  this.assertItemMatches('button', formItem);

  const headingItem = PanelBridge.calls.find(this.isHeading).item;
  this.assertItemMatches('header', headingItem);

  const landmarkItem = PanelBridge.calls.find(this.isLandmark).item;
  this.assertItemMatches('region', landmarkItem);

  const linkItem = PanelBridge.calls.find(this.isLink).item;
  this.assertItemMatches('link', linkItem);

  const tableItem = PanelBridge.calls.find(this.isTable).item;
  this.assertItemMatches('table', tableItem);
});
