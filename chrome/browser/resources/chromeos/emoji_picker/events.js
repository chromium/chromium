// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {!CustomEvent<{categoryName: string}>}
 */
export let CategoryButtonClickEvent;

export const CATEGORY_BUTTON_CLICK = 'category-button-click';

/**
 * @typedef {!CustomEvent<{group: string}>}
 */
export let GroupButtonClickEvent;

export const GROUP_BUTTON_CLICK = 'group-button-click';

/**
 * @typedef {!CustomEvent<{emoji: string, isVariant: boolean, baseEmoji: string,
 * allVariants:!Array<!string>, name:!string}>}
 */
export let EmojiButtonClickEvent;

export const EMOJI_BUTTON_CLICK = 'emoji-button-click';

/**
 * TODO(b/233130994): Update the type after removing emoji-button.
 * The current event type is used as an intermediate step for a refactor
 * leading to the removal of emoji-button. Therefore, its current state allows
 * keeping variants events for both emoji-button and emoji-group valid at the
 * same time. It will be be improved after removing emoji-button.
 */
/**
 * @typedef {!CustomEvent<{owner: ?Element,
 *            variants: ?Element, baseEmoji: String}>}
 */
export let EmojiVariantsShownEvent;

export const EMOJI_VARIANTS_SHOWN = 'emoji-variants-shown';

/**
 * @typedef {!CustomEvent}
 */
export let EmojiDataLoadedEvent;

export const EMOJI_DATA_LOADED = 'emoji-data-loaded';

/**
 * @typedef {!CustomEvent}
 */
export let EmojiRemainingDataLoadedEvent;

export const EMOJI_REMAINING_DATA_LOADED = 'emoji-data-remaining-loaded';

/**
 * @typedef {!CustomEvent}
 */
export let EmojiClearRecentClickEvent;

export const EMOJI_CLEAR_RECENTS_CLICK = 'emoji-clear-recents-click';

/**
 *
 * @typedef {!CustomEvent}
 */
export let V2ContentLoadedEvent;

export const V2_CONTENT_LOADED = 'v2-content-loaded';
/**
 * Constructs a CustomEvent with the given event type and details.
 * The event will bubble up through elements and components.
 *
 * @param {string} type event type
 * @param {T=} detail event details
 * @return {!CustomEvent<T>} custom event
 * @template T event detail type
 */
export function createCustomEvent(type, detail) {
  return new CustomEvent(type, {bubbles: true, composed: true, detail});
}
