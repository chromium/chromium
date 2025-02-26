// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages opening and closing the live caption bubble, as well
 * as moving ChromeVox focus to the bubble when it appears and restoring focus
 * when it goes away.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {SettingsManager} from '../common/settings_manager.js';

export class CaptionsHandler {
  static instance: CaptionsHandler;

  static init(): void {
    CaptionsHandler.instance = new CaptionsHandler();
  }

  static open(): void {
    if (CaptionsHandler.instance.isPrefEnabled()) {
      CaptionsHandler.instance.tryJumpToCaptionBubble_();
    } else {
      CaptionsHandler.instance.enableLiveCaption_();
    }
  }

  static close(): void {
    CaptionsHandler.instance.disableLiveCaption_();
    CaptionsHandler.instance.restoreFocus_();
  }

  isPrefEnabled(): boolean {
    return SettingsManager.getBoolean(
        'accessibility.captions.live_caption_enabled', /*isChromeVox=*/ false);
  }

  private disableLiveCaption_(): void {
    // TODO(crbug.com/395945468): Disable live caption when appropriate.
  }

  /** This function assumes the preference for live captions is false. */
  private enableLiveCaption_(): void {
    chrome.accessibilityPrivate.enableLiveCaption(true);
  }

  private restoreFocus_(): void {
    // TODO(crbug.com/395945468): Restore focus to where it was before jumping
    // to captions.
  }

  /**
   * Attempts to locate the caption bubble in the accessibility tree. If found,
   * moves focus there.
   */
  private tryJumpToCaptionBubble_(): void {
    // TODO(crbug.com/395119065): Implement logic to jump to captions.
    console.log('jump to caption bubble now');
  }
}

TestImportManager.exportForTesting(CaptionsHandler);
