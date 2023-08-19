// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Size} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

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
export interface Emoji {
  string?: string;
  visualContent?: VisualContent;
  name?: string;
  keywords?: string[];
}

export interface EmojiVariants {
  base: Emoji;
  alternates: Emoji[];
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
  url: {full: Url, preview: Url};
  previewSize: Size;
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
  preferences: {[index: string]: string};
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
