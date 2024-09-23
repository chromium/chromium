// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface to control the ChromeVox Panel.
 */
import {PanelMode} from './panel_mode.js';

// TODO(a11y): Convert to an interface once TypeScript migration is done.
export abstract class PanelInterface {
  static instance?: PanelInterface;

  /**
   * Close the menus and restore focus to the page. If a menu item's callback
   * was queued, execute it once focus is restored.
   */
  abstract closeMenusAndRestoreFocus(): Promise<void>;

  abstract get mode(): PanelMode;

  abstract setMode(mode: PanelMode): void;

  abstract get sessionState(): string;

  abstract onClose(): void;

  /**
   * A callback function to be executed to perform the action from selecting
   * a menu item after the menu has been closed and focus has been restored
   * to the page or wherever it was previously.
   */
  abstract setPendingCallback(callback: (() => Promise<void>) | null): void;
}