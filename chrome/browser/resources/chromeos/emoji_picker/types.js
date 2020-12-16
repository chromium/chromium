// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{base: number, alternates: Array<Array<number>>}}
 */
export let Emoji;

/**
 * @typedef {{group: string, emoji: Array<Emoji>}}
 */
export let EmojiGroup;

/**
 * @typedef {Array<EmojiGroup>} EmojiData
 */
export let EmojiData;