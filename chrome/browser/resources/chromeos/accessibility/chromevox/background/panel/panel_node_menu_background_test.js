// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../../../common/testing/documents.js',
  '../../testing/chromevox_e2e_test_base.js',
]);

// Fake Msgs object.
const Msgs = {
  getMsg: (id) => (id === 'panel_menu_item_none' ? 'None' : '_'),
};

// Fake PanelBridge.
const PanelBridge = {
  addMenuItem: item => PanelBridge.calls.push(item),
  calls: [],
};

/** Test fixture for PanelNodeMenuBackground. */
ChromeVoxPanelNodeMenuBackgroundTest = class extends ChromeVoxE2ETest {
  assertMenuItemIndicatesNoNodesFound(item) {
    assertNotNullNorUndefined(item);
    assertEquals('None', item.title);
    assertEquals(-1, item.callbackNodeIndex);
    assertFalse(item.isActive);
  }

  assertItemMatches(expectedName, item, opt_isActive) {
    const pattern = new RegExp(expectedName + '[_\\s]*');
    assertTrue(pattern.test(item.title));
    assertTrue(
        item.callbackNodeIndex >= 0 &&
        item.callbackNodeIndex < PanelNodeMenuBackground.callbackNodes_.length);
    assertTrue(pattern.test(
        PanelNodeMenuBackground.callbackNodes_[item.callbackNodeIndex].name));
    if (opt_isActive) {
      assertTrue(item.isActive);
    } else {
      assertFalse(item.isActive);
    }
  }

  assertMenusHaveNoNodesFoundExcept(id) {
    for (const menu of ALL_PANEL_MENU_NODE_DATA) {
      if (menu.menuId === id) {
        continue;
      }
      if (menu.menuId === PanelNodeMenuId.FORM_CONTROL) {
        // For this menu, expect to have 7 system form controls.
        const calls = PanelBridge.calls.filter(this.isFormControl);
        assertEquals(7, calls.length);
        continue;
      }

      const call = PanelBridge.calls.find(item => item.menuId === menu.menuId);
      assertNotNullNorUndefined(call);
      this.assertMenuItemIndicatesNoNodesFound(call);
    }
  }

  createAllNodeMenuBackgrounds(opt_activateMenu) {
    PanelBackground.instance.saveCurrentNode_();
    PanelBridge.calls = [];
    PanelBackground.instance.createAllNodeMenuBackgrounds_();
  }

  isFormControl(item) {
    return item.menuId === PanelNodeMenuId.FORM_CONTROL;
  }

  isHeading(item) {
    return item.menuId === PanelNodeMenuId.HEADING;
  }

  isLandmark(item) {
    return item.menuId === PanelNodeMenuId.LANDMARK;
  }

  isLink(item) {
    return item.menuId === PanelNodeMenuId.LINK;
  }

  isTable(item) {
    return item.menuId === PanelNodeMenuId.TABLE;
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
      Documents.search,
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
      Documents.header,
    ].join('\n');
  }

  get tablesDoc() {
    return [
      Documents.grid,
      Documents.table,
    ].join('\n');
  }
};

// TODO(anastasi): These tests were never run when initially written. Fix them
// so they exercise the intended code.
AX_TEST_F(
    'ChromeVoxPanelNodeMenuBackgroundTest', 'DISABLED_EmptyDocument', async function() {
      await this.runWithLoadedTree('');
      this.createAllNodeMenuBackgrounds();

      // Expect that one element is added per menu, specifying that no nodes
      // of that type are found.
      assertEquals(ALL_PANEL_MENU_NODE_DATA.length, PanelBridge.calls.length);
      // Assert all menus have a no nodes found element.
      this.assertMenusHaveNoNodesFoundExcept(null);
    });

AX_TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'DISABLED_Headings', async function() {
  await this.runWithLoadedTree(this.headingsDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu,
  // plus two extra for the additional headings found, plus six for the
  // additional system elements).
  assertEquals(
      ALL_PANEL_MENU_NODE_DATA.length + 2 + 6, PanelBridge.calls.length);

  // Expect that the three items are added to the headings menu
  const headingItems = PanelBridge.calls.filter(this.isHeading);
  assertEquals(3, headingItems.length);

  this.assertItemMatches('Heading 1', headingItems.shift());
  this.assertItemMatches('Heading 2', headingItems.shift());
  this.assertItemMatches('Heading 3', headingItems.shift());

  this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.HEADING);
});

AX_TEST_F(
    'ChromeVoxPanelNodeMenuBackgroundTest', 'DISABLED_Landmarks', async function() {
      await this.runWithLoadedTree(this.landmarksDoc);
      this.createAllNodeMenuBackgrounds();

      // Check that there are the correct number of calls (one for each menu,
      // plus seven extra for the additional landmarks found).
      assertEquals(
          ALL_PANEL_MENU_NODE_DATA.length + 7, PanelBridge.calls.length);

      // Verify that eight items were added to the landmarks menu.
      const landmarkItems = PanelBridge.calls.filter(this.isLandmark);
      assertEquals(8, landmarkItems.length);

      this.assertItemMatches('application', landmarkItems.shift());
      this.assertItemMatches('banner', landmarkItems.shift());
      this.assertItemMatches('complementary', landmarkItems.shift());
      this.assertItemMatches('form', landmarkItems.shift());
      this.assertItemMatches('main', landmarkItems.shift());
      this.assertItemMatches('navigation', landmarkItems.shift());
      this.assertItemMatches('region', landmarkItems.shift());
      this.assertItemMatches('search', landmarkItems.shift());

      this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.LANDMARK);
    });

AX_TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'DISABLED_Links', async function() {
  await this.runWithLoadedTree(this.linksDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu, plus
  // three extra for the additional links found).
  assertEquals(ALL_PANEL_MENU_NODE_DATA.length + 3, PanelBridge.calls.length);

  // Verify that four items were added to the links menu.
  const linkItems = PanelBridge.calls.filter(this.isLink);
  assertEquals(4, linkItems.length);

  this.assertItemMatches('Link 1', linkItems.shift());
  this.assertItemMatches('Link 2', linkItems.shift());
  this.assertItemMatches('Link 3', linkItems.shift());
  this.assertItemMatches('Link 4', linkItems.shift());

  this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.LINK);
});

AX_TEST_F(
    'ChromeVoxPanelNodeMenuBackgroundTest', 'DISABLED_FormControls', async function() {
      await this.runWithLoadedTree(this.formControlsDoc);
      this.createAllNodeMenuBackgrounds('panel_menu_form_controls');

      // Check that there are the correct number of calls (one for each menu,
      // plus five extra for the additional form controls found, plus seven for
      // the system elements).
      assertEquals(
          ALL_PANEL_MENU_NODE_DATA.length + 5 + 7, PanelBridge.calls.length);

      // Verify that all of the items were added to the form controls menu.
      const formItems = PanelBridge.calls.filter(this.isFormControl);
      assertEquals(6 + 7, formItems.length);

      this.assertItemMatches('button', formItems.shift());
      this.assertItemMatches('textInput', formItems.shift());
      this.assertItemMatches('textarea', formItems.shift());
      this.assertItemMatches('checkbox', formItems.shift());
      this.assertItemMatches('color.*', formItems.shift());
      this.assertItemMatches('slider[_\s\d]*', formItems.shift());

      this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.FORM_CONTROL);
    });

AX_TEST_F('ChromeVoxPanelNodeMenuBackgroundTest', 'DISABLED_Tables', async function() {
  await this.runWithLoadedTree(this.tablesDoc);
  this.createAllNodeMenuBackgrounds();

  // Check that there are the correct number of calls (one for each menu,
  // plus one extra for the additional links found).
  assertEquals(ALL_PANEL_MENU_NODE_DATA.length + 1, PanelBridge.calls.length);

  // Verify that two items were added to the tables menu.
  const tableItems = PanelBridge.calls.filter(this.isTable);
  assertEquals(2, tableItems.length);

  this.assertItemMatches('grid', tableItems.unshift());
  this.assertItemMatches('table', tableItems.unshift());

  this.assertMenusHaveNoNodesFoundExcept(PanelNodeMenuId.TABLE);
});

AX_TEST_F(
    'ChromeVoxPanelNodeMenuBackgroundTest', 'DISABLED_MixedData', async function() {
      await this.runWithLoadedTree(this.mixedDoc);
      this.createAllNodeMenuBackgrounds();

      // Check that there are the correct number of calls (one for each menu).
      assertEquals(ALL_PANEL_MENU_NODE_DATA.length, PanelBridge.calls.length);

      // Check that each item was added to the correct menu.
      const formItem = PanelBridge.calls.find(this.isFormControl);
      this.assertItemMatches('button', formItem);

      const headingItem = PanelBridge.calls.find(this.isHeading);
      this.assertItemMatches('header', headingItem);

      const landmarkItem = PanelBridge.calls.find(this.isLandmark);
      this.assertItemMatches('region', landmarkItem);

      const linkItem = PanelBridge.calls.find(this.isLink);
      this.assertItemMatches('link', linkItem);

      const tableItem = PanelBridge.calls.find(this.isTable);
      this.assertItemMatches('table', tableItem);
    });
