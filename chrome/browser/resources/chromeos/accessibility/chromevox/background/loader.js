// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Loader for the background page.
 */

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('BackgroundKeyboardHandler');
goog.require('BrailleCommandData');
goog.require('BrailleCommandHandler');
goog.require('BrailleKeyCommand');
goog.require('ChromeVoxBackground');
goog.require('ChromeVoxEditableTextBase');
goog.require('ChromeVoxState');
goog.require('CommandHandler');
goog.require('DesktopAutomationHandler');
goog.require('DownloadHandler');
goog.require('ExtensionBridge');
goog.require('FocusAutomationHandler');
goog.require('GestureCommandHandler');
goog.require('InstanceChecker');
goog.require('LocaleOutputHelper');
goog.require('MathHandler');
goog.require('NavBraille');
goog.require('Output');
goog.require('Output.EventType');
goog.require('PanelCommand');
goog.require('PhoneticData');
goog.require('constants');
goog.require('cursors.Cursor');
