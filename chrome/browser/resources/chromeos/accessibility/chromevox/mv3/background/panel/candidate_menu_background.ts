// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Presents conversion candidates, currently used by Japanese
 * IME kana-to-kanji conversion, in a ChromeVox Panel menu, reusing the same
 * menu-navigation and item-activation ChromeVox already uses for every
 * other panel menu: candidates behave like any other list of items in
 * ChromeVox, the same way a <select> behaves like any other list of items
 * on the open web.
 */
import {BridgeCallbackId} from '/common/bridge_callback_manager.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeContext} from '../../common/bridge_constants.js';
import {PanelBridge} from '../../common/panel_bridge.js';
import type {CandidateMenuItemData} from '../../common/panel_menu_data.js';
import {PhoneticData} from '../phonetic_data.js';

export class CandidateMenuBackground {
  /**
   * Opens a Panel menu listing `candidates`, one item per candidate. Each
   * item's visible label (and what gets selected/committed) is the
   * candidate itself; a per-character detailed reading, so homophone
   * candidates remain distinguishable, is carried separately as an
   * accessible name override, so it's what gets announced on focus rather
   * than what's shown as the item's label.
   * Resolves with the selected candidate, or null if the menu closed
   * without one being selected (Escape, clicking outside it, or any other
   * way the Panel's menus can close).
   */
  static async open(candidates: string[]): Promise<string|null> {
    const items: CandidateMenuItemData[] = candidates.map((candidate, i) => {
      const detail = PhoneticData.forText(candidate, 'ja') || candidate;
      const accessibleName = detail + '。' + (i + 1) + '/' + candidates.length;
      return {candidate, accessibleName};
    });
    return new Promise<string|null>((resolve, reject) => {
      const resultCallbackId = new BridgeCallbackId(
          BridgeContext.BACKGROUND,
          (selected: string|null) => resolve(selected));
      PanelBridge.showCandidateMenu(items, resultCallbackId).catch(reject);
    });
  }
}

TestImportManager.exportForTesting(CandidateMenuBackground);
