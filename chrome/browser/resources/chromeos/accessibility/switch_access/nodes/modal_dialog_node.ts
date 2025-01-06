// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '/common/event_generator.js';
import {KeyCode} from '/common/key_code.js';

import {BasicNode, BasicRootNode} from './basic_node.js';

type AutomationNode = chrome.automation.AutomationNode;

/** This class represents the group rooted at a modal dialog. */
export class ModalDialogRootNode extends BasicRootNode {
  override onExit(): void {
    // To close a modal dialog, we need to send an escape key event.
    EventGenerator.sendKeyPress(KeyCode.ESCAPE);
  }

  /**
   * Creates the tree structure for a modal dialog.
   */
  static override buildTree(dialogNode: AutomationNode): ModalDialogRootNode {
    const root = new ModalDialogRootNode(dialogNode);
    const childConstructor = (node: AutomationNode): BasicNode =>
        BasicNode.create(node, root);

    BasicRootNode.findAndSetChildren(root, childConstructor);
    return root;
  }
}
