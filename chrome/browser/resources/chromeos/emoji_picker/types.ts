// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface CategoryData {
  name: string;
  icon: string;
  active: boolean;
}

export interface Emoji {
  string: string;
  name: string;
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

export interface StoredItem {
  base: string;
  alternates: string[];
  name: string;
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
  category: string;
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
}
