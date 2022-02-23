// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{name: string, icon: string, active: boolean}} CategoryData
 */
export let CategoryData;

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

/**
 * @typedef {{base:string, alternates:!Array<!string>, name:!string}}
 * StoredItem
 */
export let StoredItem;

/**
 * @typedef {{name: string, icon: string, groupId: string, active: boolean,
 *          disabled: boolean, pagination: ?number}} SubcategoryData
 */
export let SubcategoryData;

/**
 * @enum {string}
 */
export const CategoryEnum = {
  EMOJI: 'emoji',
  EMOTICON: 'emoticon'
};
