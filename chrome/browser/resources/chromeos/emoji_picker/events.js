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
 * @typedef {!CustomEvent<{category: string}>}
 */
export let CategoryDataLoadEvent;

/**
 * The event that data of a category is fetched and processed for rendering.
 * Note: this event does not indicate if rendering of the category data is
 * completed or not.
 */
export const CATEGORY_DATA_LOADED = 'category-data-loaded';

/**
 * @typedef {!CustomEvent<{v2Enabled: boolean}>}
 */
export let EmojiPickerReadyEvent;

/**
 * The event that all the data are loaded and rendered and all the
 * emoji-picker functionalities are ready to use.
 */
export const EMOJI_PICKER_READY = 'emoji-picker-ready';

/**
 * @typedef {!CustomEvent}
 */
export let EmojiClearRecentClickEvent;

export const EMOJI_CLEAR_RECENTS_CLICK = 'emoji-clear-recents-click';

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
