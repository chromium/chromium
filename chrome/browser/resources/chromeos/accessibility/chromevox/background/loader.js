// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Loader for the background page.
 */

// These legacy Closure requires will eventually be moved to ES6 modules (see
// below block). These requires represent a dependency graph sourced from
// loader.js via a script node in our top level html. Once done, that script can
// itself be type="module".

goog.require('AbstractEarcons');
goog.require('AncestryRecoveryStrategy');
goog.require('AutomationPredicate');
goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('BrailleDisplayState');
goog.require('BrailleInterface');
goog.require('BrailleKeyCommand');
goog.require('BrailleKeyEvent');
goog.require('BridgeHelper');
goog.require('ChromeVox');
goog.require('FocusBounds');
goog.require('JaPhoneticData');
goog.require('KeyCode');
goog.require('LibLouis');
goog.require('LibLouis.FormType');
goog.require('LogStore');
goog.require('LogType');
goog.require('Msgs');
goog.require('NavBraille');
goog.require('OutputAction');
goog.require('OutputContextOrder');
goog.require('OutputEarconAction');
goog.require('OutputEventType');
goog.require('OutputNodeSpan');
goog.require('OutputSelectionSpan');
goog.require('OutputSpeechProperties');
goog.require('PanelBridge');
goog.require('PanelNodeMenuData');
goog.require('PanelTabMenuItemData');
goog.require('QueueMode');
goog.require('RecoveryStrategy');
goog.require('Spannable');
goog.require('SpeechLog');
goog.require('StringUtil');
goog.require('TextLog');
goog.require('TreeDumper');
goog.require('TreePathRecoveryStrategy');
goog.require('TtsCategory');
goog.require('TtsInterface');

goog.require('constants');
goog.require('goog.i18n.MessageFormat');

goog.require('ALL_NODE_MENU_DATA');
// ChromeVox ES6 module(s).
//
// During the transition to ES6 modules, this top level module will import all
// migrated modules. The migration will occur in breadth-first order, starting
// with background.js. ES6 modules can "depend" on Closure provides (since these
// are global), but not vice versa. Those Closure provides are above.
import('/chromevox/background/es6_loader.js');
