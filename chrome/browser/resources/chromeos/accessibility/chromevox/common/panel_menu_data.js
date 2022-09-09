// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Data types for transferring information about panel menus from
 * the background context to the panel context.
 */

import {AutomationPredicate} from '../../common/automation_predicate.js';

import {BridgeCallbackId} from './bridge_callback_manager.js';

/** @enum {number} */
export const PanelNodeMenuId = {
  HEADING: 1,
  LANDMARK: 2,
  LINK: 3,
  FORM_CONTROL: 4,
  TABLE: 5,
};

/**
 * @typedef {{
 *     menuId: !PanelNodeMenuId,
 *     titleId: string,
 *     predicate: !AutomationPredicate.Unary
 * }}
 */
export let PanelNodeMenuData;

/**
 * @typedef {{
 *     title: string,
 *     callbackId: ?BridgeCallbackId,
 *     isActive: boolean,
 *     menuId: !PanelNodeMenuId
 * }}
 */
export let PanelNodeMenuItemData;

/** @typedef {{title: string, windowId: number, tabId: number}} */
export let PanelTabMenuItemData;

export const ALL_PANEL_MENU_NODE_DATA = [
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
