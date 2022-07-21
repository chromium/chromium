// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Loader for the background page.
 */

goog.require('AbstractEarcons');
goog.require('AncestryRecoveryStrategy');
goog.require('AutomationPredicate');
goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('BaseLog');
goog.require('BrailleDisplayState');
goog.require('BrailleInterface');
goog.require('BrailleKeyCommand');
goog.require('BrailleKeyEvent');
goog.require('BridgeHelper');
goog.require('ChromeVox');
goog.require('EventLog');
goog.require('JaPhoneticData');
goog.require('KeyCode');
goog.require('LogType');
goog.require('NavBraille');
goog.require('PanelNodeMenuData');
goog.require('PanelTabMenuItemData');
goog.require('QueueMode');
goog.require('RecoveryStrategy');
goog.require('Spannable');
goog.require('SpeechLog');
goog.require('TextLog');
goog.require('TreeDumper');
goog.require('TreeLog');
goog.require('TreePathRecoveryStrategy');
goog.require('TtsCategory');
goog.require('TtsInterface');

goog.require('constants');
goog.require('goog.i18n.MessageFormat');

goog.require('ALL_NODE_MENU_DATA');
