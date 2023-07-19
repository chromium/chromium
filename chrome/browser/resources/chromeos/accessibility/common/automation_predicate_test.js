// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for automation_predicate.js. */
AccessibilityExtensionAutomationPredicateTest =
    class extends CommonE2ETestBase {
  /**@override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await Promise.all([
      await importModule(
          'AutomationPredicate', '/common/automation_predicate.js'),
      await importModule(
          'createMockNode', '/common/testing/test_node_generator.js'),
    ]);
  }
};

AX_TEST_F(
    'AccessibilityExtensionAutomationPredicateTest', 'EquivalentRoles',
    async function() {
      const site = `
    <input type="text"></input>
    <input role="combobox"></input>
  `;
      const root = await this.runWithLoadedTree(site);
      // Text field is equivalent to text field with combo box.
      const textField =
          root.find({role: chrome.automation.RoleType.TEXT_FIELD});
      assertTrue(Boolean(textField), 'No text field found.');
      const textFieldWithComboBox = root.find(
          {role: chrome.automation.RoleType.TEXT_FIELD_WITH_COMBO_BOX});
      assertTrue(
          Boolean(textFieldWithComboBox),
          'No text field with combo box found.');

      // Gather all potential predicate names.
      const keys = Object.getOwnPropertyNames(AutomationPredicate);
      for (const key of keys) {
        // Not all keys are functions or predicates e.g. makeTableCellPredicate.
        if (typeof (AutomationPredicate[key]) !== 'function' ||
            key.indexOf('make') === 0) {
          continue;
        }

        const predicate = AutomationPredicate[key];
        if (predicate(textField)) {
          assertTrue(
              Boolean(predicate(textFieldWithComboBox)),
              `Textfield with combo box should match predicate ${key}`);
        }
      }
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationPredicateTest',
    'ClickableContainersWithNoActionableDescendants', async function() {
      const site = `
      <div>
        <div aria-label="outer">
          <div aria-label='test'></div>
          <button />
        </div>
      </div>
    `;
      const root = await this.runWithLoadedTree(site);
      // Get the top level generic container.
      const container =
          root.find({role: chrome.automation.RoleType.GENERIC_CONTAINER});
      const button = root.find({role: chrome.automation.RoleType.BUTTON});
      // Make the button "clickable".
      Object.defineProperty(container, 'clickable', {value: true});
      // Arc++ doesn't set default action verb on buttons. ARC uses clickable
      // instead.
      assertEquals('press', button.defaultActionVerb);
      // Remove default action verb.
      Object.defineProperty(button, 'defaultActionVerb', {value: undefined});
      // Arc++ doesn't set default action verb on buttons.
      assertEquals(undefined, button.defaultActionVerb);
      assertFalse(AutomationPredicate.container(container));
    });

AX_TEST_F(
    'AccessibilityExtensionAutomationPredicateTest', 'PdfRootRoleAsContainer',
    async function() {
      const pdfRoot =
          createMockNode({role: chrome.automation.RoleType.PDF_ROOT});
      assertTrue(!!pdfRoot);
      assertTrue(AutomationPredicate.container(pdfRoot));
    });
