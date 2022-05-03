// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Data types for transferring information about panel menus from
 * the background context to the panel context.
 */

goog.provide('PanelNodeMenuData');
goog.provide('ALL_NODE_MENU_DATA');

goog.require('AutomationPredicate');

/** @typedef {{titleId: string, predicate: !AutomationPredicate.Unary}} */
let PanelNodeMenuData;


/** @const {!Array<!PanelNodeMenuData>} */
ALL_NODE_MENU_DATA = [
  {titleId: 'role_heading', predicate: AutomationPredicate.heading},
  {titleId: 'role_landmark', predicate: AutomationPredicate.landmark},
  {titleId: 'role_link', predicate: AutomationPredicate.link},
  {
    titleId: 'panel_menu_form_controls',
    predicate: AutomationPredicate.formField
  },
  {titleId: 'role_table', predicate: AutomationPredicate.table},
];
