// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Loads the panel script.
 */

goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('BackgroundBridge');
goog.require('BridgeHelper');
goog.require('EarconDescription');
goog.require('FocusBounds');
goog.require('KeyCode');
goog.require('LocaleOutputHelper');
goog.require('LogStore');
goog.require('Msgs');
goog.require('NavBraille');
goog.require('OutputAction');
goog.require('OutputContextOrder');
goog.require('OutputEarconAction');
goog.require('OutputEventType');
goog.require('OutputFormatParser');
goog.require('OutputFormatTree');
goog.require('OutputNodeSpan');
goog.require('OutputRoleInfo');
goog.require('OutputRulesStr');
goog.require('OutputSelectionSpan');
goog.require('OutputSpeechProperties');
goog.require('PanelNodeMenuData');
goog.require('PanelNodeMenuItemData');
goog.require('QueueMode');
goog.require('Spannable');
goog.require('TextLog');
goog.require('TtsCategory');
goog.require('ValueSelectionSpan');
goog.require('ValueSpan');

goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cursors.Unit');
goog.require('goog.i18n.MessageFormat');

goog.require('ALL_NODE_MENU_DATA');
