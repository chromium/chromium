// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



/**
 * The types of commands that can be sent between the offscreen document and the
 * ChromeVox service worker.
 */
export enum OffscreenCommandType {
  EARCON_CANCEL_PROGRESS = 'EarconCancelProgress',
  EARCON_RESET_PAN = 'EarconSesetPan',
  EARCON_SET_POSITION_FOR_RECT = 'EarconSetPositionForRect',
  IMAGE_DATA_FROM_URL = 'ImageDataFromUrl',
  LIBLOUIS_START_WORKER = 'LibLouisStartWorker',
  LIBLOUIS_RPC = 'LibLouisRPC',
  LIBLOUIS_MESSAGE = 'LibLouisMessage',
  LIBLOUIS_ERROR = 'LibLouisError',
  ON_CLIPBOARD_DATA_CHANGED = 'onClipboardDataChanged',
  ON_KEY_DOWN = 'onKeyDown',
  ON_KEY_UP = 'onKeyUp',
  ON_VOICES_CHANGED = 'onvoiceschanged',
  PLAY_EARCON = 'playEarcon',
  SHOULD_SET_DEFAULT_VOICE = 'shouldSetDefaultVoice',
}
