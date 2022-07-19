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
goog.require('BaseLog');
goog.require('BridgeHelper');
goog.require('EarconDescription');
goog.require('EventLog');
goog.require('KeyCode');
goog.require('LogType');
goog.require('NavBraille');
goog.require('PanelNodeMenuData');
goog.require('PanelNodeMenuItemData');
goog.require('QueueMode');
goog.require('RecoveryStrategy');
goog.require('Spannable');
goog.require('SpeechLog');
goog.require('TextLog');
goog.require('TreeDumper');
goog.require('TreeLog');
goog.require('TtsCategory');

goog.require('constants');
goog.require('goog.i18n.MessageFormat');

goog.require('ALL_NODE_MENU_DATA');
