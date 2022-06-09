// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Loader for the background page.
 */

goog.require('AbstractEarcons');
goog.require('AutomationPredicate');
goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('BrailleDisplayState');
goog.require('BrailleInterface');
goog.require('BrailleKeyCommand');
goog.require('BrailleKeyEvent');
goog.require('BridgeHelper');
goog.require('ChromeVox');
goog.require('CommandHandlerInterface');
goog.require('ExtraCellsSpan');
goog.require('FocusBounds');
goog.require('KeyCode');
goog.require('LibLouis');
goog.require('LibLouis.FormType');
goog.require('LocaleOutputHelper');
goog.require('LogStore');
goog.require('LogType');
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
goog.require('PanelBridge');
goog.require('PanelNodeMenuData');
goog.require('PanelTabMenuItemData');
goog.require('PhoneticData');
goog.require('QueueMode');
goog.require('Spannable');
goog.require('SpeechLog');
goog.require('StringUtil');
goog.require('TextLog');
goog.require('TreeDumper');
goog.require('TreePathRecoveryStrategy');
goog.require('TtsCategory');
goog.require('TtsInterface');
goog.require('ValueSelectionSpan');
goog.require('ValueSpan');

goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cursors.Unit');
goog.require('goog.i18n.MessageFormat');

goog.require('ALL_NODE_MENU_DATA');
