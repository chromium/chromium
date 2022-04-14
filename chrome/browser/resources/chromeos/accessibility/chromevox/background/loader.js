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
goog.require('AutomationObjectConstructorInstaller');
goog.require('BrailleDisplayState');
goog.require('BrailleInterface');
goog.require('BrailleKeyCommand');
goog.require('BrailleKeyEvent');
goog.require('ChromeVox');
goog.require('ChromeVoxState');
goog.require('ChromeVoxStateObserver');
goog.require('CommandHandlerInterface');
goog.require('EventSourceState');
goog.require('EventStreamLogger');
goog.require('ExtensionBridge');
goog.require('ExtraCellsSpan');
goog.require('JaPhoneticMap');
goog.require('KeyCode');
goog.require('KeySequence');
goog.require('LibLouis');
goog.require('LibLouis.FormType');
goog.require('LocaleOutputHelper');
goog.require('LogStore');
goog.require('Msgs');
goog.require('NavBraille');
goog.require('Output');
goog.require('OutputEventType');
goog.require('PanelCommand');
goog.require('PhoneticData');
goog.require('QueueMode');
goog.require('Spannable');
goog.require('SpeechLog');
goog.require('StringUtil');
goog.require('TreeDumper');
goog.require('TreePathRecoveryStrategy');
goog.require('TtsInterface');
goog.require('ValueSelectionSpan');
goog.require('ValueSpan');

goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
