// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Codepoints} from './types.js';

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

/**
 * @typedef {!CustomEvent}
 */
export let DataLoadedEvent;

export const DATA_LOADED_EVENT = 'data-loaded';

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