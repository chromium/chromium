// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The types of commands that can be sent between the offscreen document and the
 * Accessibility Common service workers.
 */
export enum OffscreenCommandType {
  DICTATION_PLAY_CANCEL = 'DictationPlayCancel',
  DICTATION_PLAY_START = 'DictationPlayStart',
  DICTATION_PLAY_END = 'DictationPlayEnd',
  DICTATION_PUMPKIN_INSTALL = 'DictationPumpkinInstall',
  DICTATION_PUMPKIN_RECEIVE = 'DictationPumpkinReceive',
  DICTATION_PUMPKIN_SEND = 'DictationPumpkinSend',
}
