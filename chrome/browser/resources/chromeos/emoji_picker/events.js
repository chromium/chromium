// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {!CustomEvent<{group: string}>}
 */
export let GroupButtonClickEvent;

export const GROUP_BUTTON_CLICK = 'group-button-click';

/**
 * @typedef {!CustomEvent<{emoji: string, isVariant: boolean}>}
 */
export let EmojiButtonClickEvent;

export const EMOJI_BUTTON_CLICK = 'emoji-button-click';

/**
 * @typedef {!CustomEvent<{button: ?Element, variants: ?Element}>}
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
