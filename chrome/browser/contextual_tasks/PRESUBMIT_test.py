#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(
  os.path.abspath(os.path.dirname(os.path.abspath(__file__)) + '/../../..')
)
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi
from PRESUBMIT_test_mocks import MockOutputApi


_CONTEXTUAL_TASKS_UI_CC = (
  'chrome/browser/contextual_tasks/contextual_tasks_ui.cc'
)


class ContextualTasksUiNoNewMethodsPresubmitTest(unittest.TestCase):
  def testWarnsOnNewContextualTasksUiMethod(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        [
          'void ContextualTasksUI::MyNewMethod() {',
          '  DoSomething();',
          '}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'warning')
    self.assertEqual(
      results[0].items,
      [f'  {_CONTEXTUAL_TASKS_UI_CC}:1: ContextualTasksUI::MyNewMethod()'],
    )

  def testWarnsOnWrappedReturnTypeAndNestedClassMethod(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        [
          'const std::optional<base::Uuid>&',
          'ContextualTasksUI::GetNewTaskId() {',
          '  return task_id_;',
          '}',
          'void ContextualTasksUI::FrameNavObserver::OnNewEvent() {',
          '}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 1)
    self.assertEqual(
      results[0].items,
      [
        f'  {_CONTEXTUAL_TASKS_UI_CC}:2: ContextualTasksUI::GetNewTaskId()',
        f'  {_CONTEXTUAL_TASKS_UI_CC}:5: '
        'ContextualTasksUI::FrameNavObserver::OnNewEvent()',
      ],
    )

  def testWarnsOnNewAnonymousNamespaceFunction(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        [
          'bool ShouldDoSomethingNew(const GURL& url) {',
          '  return url.is_valid();',
          '}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 1)
    self.assertEqual(
      results[0].items,
      [f'  {_CONTEXTUAL_TASKS_UI_CC}:1: ShouldDoSomethingNew()'],
    )

  def testAllowsModifyingExistingMethod(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        new_contents=[
          'void ContextualTasksUI::CloseSidePanel(bool animate) {',
          '  DoClose(animate);',
          '}',
        ],
        old_contents=[
          'void ContextualTasksUI::CloseSidePanel() {',
          '  DoClose();',
          '}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 0)

  def testWarnsWhenAddingNewOverloadOfExistingMethod(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        new_contents=[
          'void ContextualTasksUI::CloseSidePanel() {}',
          'void ContextualTasksUI::CloseSidePanel(bool animate) {}',
        ],
        old_contents=[
          'void ContextualTasksUI::CloseSidePanel() {}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 1)
    self.assertEqual(len(results[0].items), 1)

  def testAllowsIndentedCallsMacrosAndComments(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        [
          '// ContextualTasksUI::CommentedOutMethod() {',
          'DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksUI,',
          '                                      kToolbarElementId);',
          'WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUI)',
          '  if (!ContextualTasksUI::AreUrlsEqual(a, b)) {',
          '    base::BindRepeating(&ContextualTasksUI::OnComplete,',
          '                        weak_ptr_factory_.GetWeakPtr());',
          '  }',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 0)

  def testAllowsBypassFooterInDescription(self):
    input_api = MockInputApi()
    input_api.change.footers['Allow-Contextual-Tasks-Ui-Changes'] = [
      'Needed for legacy webview crash fix'
    ]
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        [
          'void ContextualTasksUI::AllowedOverride() {',
          '}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 0)

  def testWarnsWhenBypassFooterHasEmptyReason(self):
    input_api = MockInputApi()
    input_api.change.footers['Allow-Contextual-Tasks-Ui-Changes'] = ['   ']
    input_api.files = [
      MockAffectedFile(
        _CONTEXTUAL_TASKS_UI_CC,
        [
          'void ContextualTasksUI::AllowedOverride() {',
          '}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 1)

  def testIgnoresOtherFiles(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(
        'chrome/browser/contextual_tasks/contextual_tasks_ui_base.cc',
        [
          'void ContextualTasksUIBase::MyNewMethod() {',
          '}',
        ],
      ),
      MockAffectedFile(
        'chrome/browser/contextual_tasks/'
        'contextual_tasks_ui_post_rearchitecture.cc',
        [
          'void ContextualTasksUIPostRearchitecture::MyNewMethod() {',
          '}',
        ],
      ),
    ]
    results = PRESUBMIT.CheckNoNewMethodsInContextualTasksUi(
      input_api, MockOutputApi()
    )
    self.assertEqual(len(results), 0)


if __name__ == '__main__':
  unittest.main()
