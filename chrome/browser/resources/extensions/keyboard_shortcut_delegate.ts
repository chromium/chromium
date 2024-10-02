// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface KeyboardShortcutDelegate {
  /**
   * Called when shortcut capturing changes in order to suspend or re-enable
   * global shortcut handling. This is important so that the shortcuts aren't
   * processed normally as the user types them.
   * TODO(devlin): From very brief experimentation, it looks like preventing
   * the default handling on the event also does this. Investigate more in the
   * future.
   */
  setShortcutHandlingSuspended(isCapturing: boolean): void;

  /**
   * Updates an extension command's keybinding.
   */
  updateExtensionCommandKeybinding(
      extensionId: string, commandName: string, keybinding: string): void;

  /**
   * Updates an extension command's scope.
   */
  updateExtensionCommandScope(
      extensionId: string, commandName: string,
      scope: chrome.developerPrivate.CommandScope): void;
}

class DummyKeyboardShortcutDelegate implements KeyboardShortcutDelegate {
  setShortcutHandlingSuspended(_isCapturing: boolean) {}
  updateExtensionCommandKeybinding(
      _extensionId: string, _commandName: string, _keybinding: string) {}
  updateExtensionCommandScope(
      _extensionId: string, _commandName: string,
      _scope: chrome.developerPrivate.CommandScope) {}
}

export function createDummyKeyboardShortcutDelegate():
    KeyboardShortcutDelegate {
  return new DummyKeyboardShortcutDelegate();
}
