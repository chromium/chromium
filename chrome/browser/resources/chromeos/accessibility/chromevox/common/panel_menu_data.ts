// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Data types for transferring information about panel menus from
 * the background context to the panel context.
 */

import {AutomationPredicate} from '/common/automation_predicate.js';
import {BridgeCallbackId} from '/common/bridge_callback_manager.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

export enum PanelNodeMenuId {
  HEADING = 1,
  LANDMARK = 2,
  LINK = 3,
  FORM_CONTROL = 4,
  TABLE = 5,
}

export interface PanelNodeMenuData {
  menuId: PanelNodeMenuId;
  titleId: string;
  predicate: AutomationPredicate.Unary;
}

export interface PanelNodeMenuItemData {
  title: string;
  callbackId: BridgeCallbackId | null;
  isActive: boolean;
  menuId: PanelNodeMenuId;
}

export interface PanelTabMenuItemData {
  title: string;
  windowId: number;
  tabId: number;
}

export const ALL_PANEL_MENU_NODE_DATA: PanelNodeMenuData[] = [
  {
    menuId: PanelNodeMenuId.HEADING,
    titleId: 'role_heading',
    predicate: AutomationPredicate.heading,
  },
  {
    menuId: PanelNodeMenuId.LANDMARK,
    titleId: 'role_landmark',
    predicate: AutomationPredicate.landmark,
  },
  {
    menuId: PanelNodeMenuId.LINK,
    titleId: 'role_link',
    predicate: AutomationPredicate.link,
  },
  {
    menuId: PanelNodeMenuId.FORM_CONTROL,
    titleId: 'panel_menu_form_controls',
    predicate: AutomationPredicate.formField,
  },
  {
    menuId: PanelNodeMenuId.TABLE,
    titleId: 'role_table',
    predicate: AutomationPredicate.table,
  },
];

TestImportManager.exportForTesting(
    ['PanelNodeMenuId', PanelNodeMenuId],
    ['ALL_PANEL_MENU_NODE_DATA', ALL_PANEL_MENU_NODE_DATA]);
