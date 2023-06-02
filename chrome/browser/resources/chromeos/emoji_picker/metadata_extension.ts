// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * TODO(b/233271528): Remove metadata_extension module.
 * This module includes tab details which is better to be extracted dynamically
 * from the emoji group data directly.
 */


import {CategoryData, CategoryEnum, GifSubcategoryData, SubcategoryData} from './types.js';

const RECENTLY_USED_NAME = 'Recently used';

/**
 * Creates a list of tabs with all required information for rendering given
 * category orders and basic info of each tab. It also adds tabs for "Recently
 * used" group as the first tab of each category.
 */
function makeGroupTabs(
    categories: CategoryEnum[],
    categoryBaseEmojis: Record<string, Array<{
                                 name: string,
                                 pagination?: number,
                                 icon?: string,
                               }>>) {
  const groupTabs: SubcategoryData[] = [];
  let groupId = 0;

  // TODO(b/216190190): Change groupId to number type.
  for (const category of categories) {
    // Add recently used tab.
    groupTabs.push(
        {
          name: RECENTLY_USED_NAME,
          icon: 'emoji_picker_v2:emoji_recent',
          category: category,
          groupId: `${category}-history`,
          active: false,
          disabled: true,
          pagination: 1,
        },
    );
    let pagination = 1;
    categoryBaseEmojis[category]?.forEach(
        tab => {
          // Update pagination if provided.
          pagination = tab.pagination || pagination;
          // Add new tab.
          groupTabs.push(
              {
                name: tab.name,
                icon: tab.icon,
                category: category,
                pagination: pagination,
                groupId: groupId.toString(),
                active: false,
                disabled: false,
              },
          );
          groupId++;
        },
    );
  }
  return groupTabs;
}

export const CATEGORY_METADATA: CategoryData[] = [
      {
        name: CategoryEnum.EMOJI,
        icon: 'emoji_picker_v2:emoji_emojis_outlined',
        active: true,
      },
      {
        name: CategoryEnum.SYMBOL,
        icon: 'emoji_picker_v2:emoji_symbols',
        active: false,
      },
      {
        name: CategoryEnum.EMOTICON,
        icon: 'emoji_picker_v2:emoji_emoticons',
        active: false,
      },
    ];

export const GIF_CATEGORY_METADATA: CategoryData[] = [
      ...CATEGORY_METADATA,
      {
        name: CategoryEnum.GIF,
        icon: 'emoji_picker_v2:emoji_gif',
        active: false,
      },
    ];

export const CATEGORY_TABS = {
  'emoji': [
    {
      name: 'Smileys & Emotions',
      icon: 'emoji_picker_v2:emoji_emojis_filled',
      pagination: 1,
    },
    {
      name: 'People',
      icon: 'emoji_picker:emoji_people',
    },
    {
      name: 'Animals & Nature',
      icon: 'emoji_picker:emoji_nature',
    },
    {
      name: 'Food & Drink',
      icon: 'emoji_picker:emoji_food_beverage',
    },
    {
      name: 'Travel & Places',
      icon: 'emoji_picker:emoji_transportation',
    },
    {
      name: 'Activities & Events',
      icon: 'emoji_picker:emoji_events',
    },
    {
      name: 'Objects',
      icon: 'emoji_picker:emoji_objects',
    },
    {
      name: 'Symbols',
      icon: 'emoji_picker:emoji_symbols',
    },
    {
      name: 'Flags',
      icon: 'emoji_picker:flag',
    },
  ],
  'emoticon': [
    {name: 'Classic', pagination: 1},
    {name: 'Smiling'},
    {name: 'Love'},
    {name: 'Hugging'},
    {name: 'Flexing'},
    {name: 'Animals', pagination: 2},
    {name: 'Surprising'},
    {name: 'Dancing'},
    {name: 'Shrugging'},
    {name: 'Table Flipping', pagination: 3},
    {name: 'Disapproving'},
    {name: 'Crying'},
    {name: 'Worrying', pagination: 4},
    {name: 'Pointing'},
    {name: 'Sparkling'},
  ],
  'symbol': [
    {name: 'Arrows', pagination: 1},
    {name: 'Bullet/Stars'},
    {name: 'Currency'},
    {name: 'Letterlike', pagination: 2},
    {name: 'Math'},
    {name: 'Miscellaneous'},
  ],
};

// TODO(b/233271528): Remove the list and load it from the input data.
/**
 * The list of tabs based on the order of category buttons and basic tab info
 * of each category.
 */
export const SUBCATEGORY_TABS = makeGroupTabs(
    CATEGORY_METADATA.map(item => item.name),
    CATEGORY_TABS,
);

const SUBCATEGORY_TABS_GIF_SUPPORT = makeGroupTabs(
    GIF_CATEGORY_METADATA.map(item => item.name),
    CATEGORY_TABS,
);

export const gifCategoryTabs =
    (categories: Record<string, GifSubcategoryData[]>) => makeGroupTabs(
        GIF_CATEGORY_METADATA.map(item => item.name),
        categories,
    );

// A mapping from each category to the index of their first tab.
export const TABS_CATEGORY_START_INDEX: Map<CategoryEnum, number> = new Map(
    SUBCATEGORY_TABS.map((item, index) => [item.category, index] as const)
        .reverse());

export const TABS_CATEGORY_START_INDEX_GIF_SUPPORT: Map<CategoryEnum, number> =
    new Map(SUBCATEGORY_TABS_GIF_SUPPORT
                .map((item, index) => [item.category, index] as const)
                .reverse());

export const EMOJI_GROUP_TABS =
    makeGroupTabs([CategoryEnum.EMOJI], CATEGORY_TABS);
