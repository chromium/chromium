// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Loads the panel script.
 */

goog.require('AncestryRecoveryStrategy');
goog.require('AutomationPredicate');
goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('BackgroundBridge');
goog.require('BridgeHelper');
goog.require('EarconDescription');
goog.require('FocusBounds');
goog.require('KeyCode');
goog.require('LogStore');
goog.require('Msgs');
goog.require('NavBraille');
goog.require('OutputAction');
goog.require('OutputContextOrder');
goog.require('OutputEarconAction');
goog.require('OutputEventType');
goog.require('OutputNodeSpan');
goog.require('OutputSelectionSpan');
goog.require('OutputSpeechProperties');
goog.require('PanelNodeMenuData');
goog.require('PanelNodeMenuItemData');
goog.require('QueueMode');
goog.require('RecoveryStrategy');
goog.require('Spannable');
goog.require('StringUtil');
goog.require('TextLog');
goog.require('TtsCategory');

goog.require('constants');
goog.require('goog.i18n.MessageFormat');

goog.require('ALL_NODE_MENU_DATA');
