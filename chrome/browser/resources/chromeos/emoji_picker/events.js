// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {!CustomEvent<{group: string}>}
 */
export let GroupButtonEvent;

export const GROUP_BUTTON_EVENT = 'group-button';

/**
 * @typedef {!CustomEvent<{emoji: string}>}
 */
export let EmojiButtonEvent;

export const EMOJI_BUTTON_EVENT = 'emoji-button';