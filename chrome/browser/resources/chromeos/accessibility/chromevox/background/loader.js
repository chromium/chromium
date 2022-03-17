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
goog.require('AutoScrollHandler');
goog.require('AutomationObjectConstructorInstaller');
goog.require('BackgroundKeyboardHandler');
goog.require('BaseAutomationHandler');
goog.require('BrailleBackground');
goog.require('BrailleCaptionsBackground');
goog.require('BrailleCommandData');
goog.require('BrailleKeyCommand');
goog.require('ChromeVox');
goog.require('ChromeVoxEditableTextBase');
goog.require('ChromeVoxKbHandler');
goog.require('ChromeVoxPrefs');
goog.require('ChromeVoxState');
goog.require('ChromeVoxStateObserver');
goog.require('CommandHandlerInterface');
goog.require('CommandStore');
goog.require('CompositeTts');
goog.require('ConsoleTts');
goog.require('CustomAutomationEvent');
goog.require('EventGenerator');
goog.require('EventSourceState');
goog.require('EventStreamLogger');
goog.require('ExtensionBridge');
goog.require('GestureCommandData');
goog.require('GestureGranularity');
goog.require('JaPhoneticMap');
goog.require('KeyCode');
goog.require('LibLouis.FormType');
goog.require('LocaleOutputHelper');
goog.require('LogStore');
goog.require('MathHandler');
goog.require('NavBraille');
goog.require('Output');
goog.require('OutputEventType');
goog.require('PanelCommand');
goog.require('PhoneticData');
goog.require('QueueMode');
goog.require('TreeDumper');
goog.require('TreePathRecoveryStrategy');
goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('editing.EditableLine');
