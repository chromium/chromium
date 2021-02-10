// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{string: string, name: string, keywords: !Array<!string>}} Emoji
 */
export let Emoji;

/**
 * @typedef {{base: Emoji, alternates: !Array<Emoji>}} EmojiVariants
 */
export let EmojiVariants;

/**
 * @typedef {{group: string, emoji: !Array<EmojiVariants>}} EmojiGroup
 */
export let EmojiGroup;

/**
 * @typedef {Array<EmojiGroup>} EmojiGroupData
 */
export let EmojiGroupData;
