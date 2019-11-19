// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @interface */
export class KeyboardShortcutDelegate {
  /**
   * Called when shortcut capturing changes in order to suspend or re-enable
   * global shortcut handling. This is important so that the shortcuts aren't
   * processed normally as the user types them.
   * TODO(devlin): From very brief experimentation, it looks like preventing
   * the default handling on the event also does this. Investigate more in the
   * future.
   * @param {boolean} isCapturing
   */
  setShortcutHandlingSuspended(isCapturing) {}

  /**
   * Updates an extension command's keybinding.
   * @param {string} extensionId
   * @param {string} commandName
   * @param {string} keybinding
   */
  updateExtensionCommandKeybinding(extensionId, commandName, keybinding) {}

  /**
   * Updates an extension command's scope.
   * @param {string} extensionId
   * @param {string} commandName
   * @param {chrome.developerPrivate.CommandScope} scope
   */
  updateExtensionCommandScope(extensionId, commandName, scope) {}
}
