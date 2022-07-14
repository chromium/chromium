// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{name: string, icon: string, active: boolean}} CategoryData
 */
export let CategoryData;

/**
 * @typedef {{string: string, name: string, keywords: ?Array<!string>}} Emoji
 */
export let Emoji;

/**
 * @typedef {{base: Emoji, alternates: !Array<Emoji>}} EmojiVariants
 */
export let EmojiVariants;

/**
 * @typedef {{category: CategoryEnum, group: string,
 *            emoji: !Array<EmojiVariants>}} EmojiGroup
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
 * @typedef {{name: string, category: string, emoji: Array<EmojiVariants>,
 *            groupId: string, activate: boolean, disabled: boolean,
 *            pagination: ?number, preferences: Object<string,string>,
 *            isHistory: boolean}} EmojiGroupElement
 */
export let EmojiGroupElement;

/**
 * @enum {string}
 */
export const CategoryEnum = {
  EMOJI: 'emoji',
  EMOTICON: 'emoticon',
  SYMBOL: 'symbol',
};
