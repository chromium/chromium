// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Size} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

// LINT.IfChange

// `DEFAULT` is not defined in tools/emoji_data.py for `Tone` and `Gender`
// because it is only relevant for frontend persistence.

export enum Tone {
  DEFAULT = 0,
  LIGHT = 1,
  MEDIUM_LIGHT = 2,
  MEDIUM = 3,
  MEDIUM_DARK = 4,
  DARK = 5,
}

export enum Gender {
  DEFAULT = 0,
  WOMAN = 1,
  MAN = 2,
}

// LINT.ThenChange(//chromeos/ash/components/emoji/tools/emoji_data.py)

export type PreferenceMapping = Record<string, string>;

export interface CategoryData {
  name: CategoryEnum;
  icon: string;
  active: boolean;
}

// string is defined in Emoji only when Emoji is of type Emoji, Emoticon or
// Symbol. string represents the Emoji to be inserted in string form, e.g. "😂".
// visualContent is defined in Emoji only when Emoji is of type GIF.
// visualContent represents the information needed to display visual content
// such as GIF (see VisualContent interface below).
// tone and gender are defined in Emoji only when it's of type Emoji, and only
// for variants of applicable emojis. They can never have the value DEFAULT
// because the fields are omitted in this case, but DEFAULT is relevant for
// persistence.
export interface Emoji {
  string?: string;
  visualContent?: VisualContent;
  name?: string;
  keywords?: string[];
  tone?: Tone;
  gender?: Gender;
}

// When `groupedTone` is true, all emojis that also have it set to true will
// update to have the same tone. The same applies to `groupedGender`.
export interface EmojiVariants {
  base: Emoji;
  alternates: Emoji[];
  groupedTone?: boolean;
  groupedGender?: boolean;
}

export interface EmojiHistoryItem extends EmojiVariants {
  // Timestamp is in milliseconds since unix epoch.
  timestamp?: number;
}

export interface EmojiGroup {
  category: CategoryEnum;
  group: string;
  searchOnly?: boolean;
  emoji: EmojiVariants[];
}

export type EmojiGroupData = EmojiGroup[];

export interface VisualContent {
  id: string;
  url: {full: Url, preview: Url, previewImage: Url};
  previewSize: Size;
  // `fullSize` is currently unused by Emoji Picker.
}

export interface SubcategoryData {
  name: string;
  icon?: string;
  groupId: string;
  active: boolean;
  disabled: boolean;
  pagination?: number;
  category: CategoryEnum;
}

export interface EmojiGroupElement {
  name: string;
  category: CategoryEnum;
  emoji: EmojiVariants[];
  groupId: string;
  active: boolean;
  disabled: boolean;
  pagination?: number;
  preferences: PreferenceMapping;
  isHistory: boolean;
}

export enum CategoryEnum {
  EMOJI = 'emoji',
  EMOTICON = 'emoticon',
  SYMBOL = 'symbol',
  GIF = 'gif',
}

export interface GifSubcategoryData {
  name: string;
  pagination?: number;
  icon?: string;
}
