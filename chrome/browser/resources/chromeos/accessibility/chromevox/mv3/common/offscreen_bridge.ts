// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for communication with the offscreen
 * document.
 */

import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from './bridge_constants.js';
import type {EarconId} from './earcon_id.js';
import type {ClipboardData, StateWithMaxCellHeight} from './offscreen_bridge_constants.js';

const OffscreenTarget = BridgeConstants.Offscreen.TARGET;
const OffscreenAction = BridgeConstants.Offscreen.Action;

const OffscreenTestTarget = BridgeConstants.OffscreenTest.TARGET;
const OffscreenTestAction = BridgeConstants.OffscreenTest.Action;

export class OffscreenBridge {
  static chromeVoxReady(): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.CHROMEVOX_READY);
  }

  static earconCancelProgress(): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.EARCON_CANCEL_PROGRESS);
  }

  static earconCancelLoading(): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.EARCON_CANCEL_LOADING)
  }

  static earconResetPan(): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.EARCON_RESET_PAN);
  }

  static earconSetPositionForRect(
      rect: chrome.automation.Rect,
      container: chrome.automation.Rect): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.EARCON_SET_POSITION_FOR_RECT, rect,
        container);
  }

  static imageDataFromUrl(
      imageDataUrl: string,
      imageState: StateWithMaxCellHeight): Promise<string> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.IMAGE_DATA_FROM_URL, imageDataUrl,
        imageState);
  }

  static learnModeRegisterListeners(): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.LEARN_MODE_REGISTER_LISTENERS);
  }

  static learnModeRemoveListeners(): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.LEARN_MODE_REMOVE_LISTENERS);
  }

  static libLouisStartWorker(wasmPath: string): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.LIBLOUIS_START_WORKER, wasmPath);
  }

  static libLouisRPC(json: string): Promise<{message?: string}> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.LIBLOUIS_RPC, json);
  }

  static onClipboardDataChanged(forceRead: boolean): Promise<ClipboardData> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.ON_CLIPBOARD_DATA_CHANGED, forceRead);
  }

  static playEarcon(earconId: EarconId): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.PLAY_EARCON, earconId);
  }

  static shouldSetDefaultVoice(): Promise<boolean> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.SHOULD_SET_DEFAULT_VOICE);
  }

  static sreMove(keyCode: number): Promise<string> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.SRE_MOVE, keyCode);
  }

  static sreWalk(mathml: string): Promise<string> {
    return BridgeHelper.sendMessage(
        OffscreenTarget, OffscreenAction.SRE_WALK, mathml);
  }

  // For testing purposes only.

  static recordEarconsForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        OffscreenTestTarget, OffscreenTestAction.RECORD_EARCONS);
  }

  static reportEarconsForTest(): Promise<EarconId[]> {
    return BridgeHelper.sendMessage(
        OffscreenTestTarget, OffscreenTestAction.REPORT_EARCONS);
  }
}

TestImportManager.exportForTesting(OffscreenBridge);
