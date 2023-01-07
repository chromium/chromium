// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export enum KeyboardKeyState {
  /** The key has not been pressed during this test session. */
  NOT_PRESSED = 'not-pressed',
  /** The key is currently pressed. */
  PRESSED = 'pressed',
  /** The key is not currently pressed, but we've seen it pressed previously. */
  TESTED = 'tested',
}
