// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const CATEGORY_METADATA = [
  {
    name: 'emoji',
    icon: 'emoji_picker:insert_emoticon',
    active: true,
  },
  // TODO(b/213141035): insert symbols metadata back.
  {
    name: 'emoticon',
    icon: 'emoji_picker_v2:emoticon_gssmiley',
    active: false,
  },
];

// TODO(b/216190190): Change groupId to number type.
export const V2_SUBCATEGORY_TABS = [
  {
    name: 'Recently Used',
    icon: 'emoji_picker:schedule',
    category: 'emoji',
    groupId: 'history',
    active: false,
    disabled: true,
    pagination: 1
  },
  {
    name: 'Smileys & Emotion',
    icon: 'emoji_picker:insert_emoticon',
    category: 'emoji',
    groupId: '0',
    active: false,
    disabled: false
  },
  {
    name: 'People',
    icon: 'emoji_picker:emoji_people',
    category: 'emoji',
    groupId: '1',
    active: false,
    disabled: false
  },
  {
    name: 'Animals & Nature',
    icon: 'emoji_picker:emoji_nature',
    category: 'emoji',
    groupId: '2',
    active: false,
    disabled: false
  },
  {
    name: 'Food & Drink',
    icon: 'emoji_picker:emoji_food_beverage',
    category: 'emoji',
    groupId: '3',
    active: false,
    disabled: false
  },
  {
    name: 'Travel & Places',
    icon: 'emoji_picker:emoji_transportation',
    category: 'emoji',
    groupId: '4',
    active: false,
    disabled: false
  },
  {
    name: 'Activities',
    icon: 'emoji_picker:emoji_events',
    category: 'emoji',
    groupId: '5',
    active: false,
    disabled: false
  },
  {
    name: 'Objects',
    icon: 'emoji_picker:emoji_objects',
    category: 'emoji',
    groupId: '6',
    active: false,
    disabled: false
  },
  {
    name: 'Symbols',
    icon: 'emoji_picker:emoji_symbols',
    category: 'emoji',
    groupId: '7',
    active: false,
    disabled: false
  },
  {
    name: 'Flags',
    icon: 'emoji_picker:flag',
    category: 'emoji',
    groupId: '8',
    active: false,
    disabled: false
  },
  {
    name: 'Recently Used',
    icon: 'emoji_picker:schedule',
    category: 'emoticon',
    groupId: 'emoticon-history',
    active: false,
    disabled: true,
    pagination: 1
  },
  {
    name: 'Classic',
    category: 'emoticon',
    groupId: '9',
    active: false,
    disabled: false,
    pagination: 1
  },
  {
    name: 'Smiling',
    category: 'emoticon',
    groupId: '10',
    active: false,
    disabled: false,
    pagination: 1
  },
  {
    name: 'Loving',
    category: 'emoticon',
    groupId: '11',
    active: false,
    disabled: false,
    pagination: 1
  },
  {
    name: 'Hugging',
    category: 'emoticon',
    groupId: '12',
    active: false,
    disabled: false,
    pagination: 1
  },
  {
    name: 'Flexing',
    category: 'emoticon',
    groupId: '13',
    active: false,
    disabled: false,
    pagination: 1
  },
  {
    name: 'Animals',
    category: 'emoticon',
    groupId: '14',
    active: false,
    disabled: false,
    pagination: 2
  },
  {
    name: 'Surprising',
    category: 'emoticon',
    groupId: '15',
    active: false,
    disabled: false,
    pagination: 2
  },
  {
    name: 'Dancing',
    category: 'emoticon',
    groupId: '16',
    active: false,
    disabled: false,
    pagination: 2
  },
  {
    name: 'Shrugging',
    category: 'emoticon',
    groupId: '17',
    active: false,
    disabled: false,
    pagination: 2
  },
  {
    name: 'Table flipping',
    category: 'emoticon',
    groupId: '18',
    active: false,
    disabled: false,
    pagination: 3
  },
  {
    name: 'Disapproving',
    category: 'emoticon',
    groupId: '19',
    active: false,
    disabled: false,
    pagination: 3
  },
  {
    name: 'Crying',
    category: 'emoticon',
    groupId: '20',
    active: false,
    disabled: false,
    pagination: 3
  },
  {
    name: 'Worrying',
    category: 'emoticon',
    groupId: '21',
    active: false,
    disabled: false,
    pagination: 4
  },
  {
    name: 'Pointing',
    category: 'emoticon',
    groupId: '22',
    active: false,
    disabled: false,
    pagination: 4
  },
  {
    name: 'Sparkling',
    category: 'emoticon',
    groupId: '23',
    active: false,
    disabled: false,
    pagination: 4
  },
];
