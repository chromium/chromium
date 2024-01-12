// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// height and width should match the dialog size in EmojiPickerDialog.
export const EMOJI_NUM_TABS_IN_FIRST_PAGE = 8;
export const EMOJI_HIGHLIGHTER_WIDTH = 24;
export const EMOJI_PICKER_TOP_PADDING = 10;
export const EMOJI_PICKER_HEIGHT = 480;
export const EMOJI_PICKER_WIDTH = 420;
export const EMOJI_PICKER_SIDE_PADDING = 16;
export const EMOJI_PER_ROW = 9;
export const GROUP_ICON_SIZE =
    (EMOJI_PICKER_WIDTH - 2 * EMOJI_PICKER_SIDE_PADDING) / EMOJI_PER_ROW;
export const EMOJI_ICON_SIZE = 32;
const VISUAL_CONTENT_PADDING = 8;
export const VISUAL_CONTENT_WIDTH =
    (EMOJI_PICKER_WIDTH - 2 * EMOJI_PICKER_SIDE_PADDING) / 2 -
    VISUAL_CONTENT_PADDING / 2;

export const EMOJI_SPACING =
    (EMOJI_PICKER_WIDTH - 2 * EMOJI_PICKER_SIDE_PADDING -
     EMOJI_PER_ROW * EMOJI_ICON_SIZE) /
    (EMOJI_PER_ROW - 1);
export const EMOJI_HIGHLIGHTER_WIDTH_PX = `${EMOJI_HIGHLIGHTER_WIDTH}px`;
export const EMOJI_PICKER_HEIGHT_PX = `${EMOJI_PICKER_HEIGHT}px`;
export const EMOJI_PICKER_WIDTH_PX = `${EMOJI_PICKER_WIDTH}px`;
export const EMOJI_SIZE_PX = `${EMOJI_ICON_SIZE}px`;
export const EMOJI_SPACING_PX = `${EMOJI_SPACING}px`;
export const EMOJI_GROUP_SIZE_PX = `${GROUP_ICON_SIZE}px`;
export const EMOJI_PICKER_SIDE_PADDING_PX = `${EMOJI_PICKER_SIDE_PADDING}px`;
export const EMOJI_PICKER_TOP_PADDING_PX = `${EMOJI_PICKER_TOP_PADDING}px`;
export const VISUAL_CONTENT_PADDING_PX = `${VISUAL_CONTENT_PADDING}px`;
export const VISUAL_CONTENT_WIDTH_PX = `${VISUAL_CONTENT_WIDTH}px`;

export const GROUP_PER_ROW = 10;
export const EMOJI_GROUP_SPACING =
    (EMOJI_PICKER_WIDTH - 2 * EMOJI_PICKER_SIDE_PADDING -
     GROUP_PER_ROW * EMOJI_ICON_SIZE) /
    (GROUP_PER_ROW - 1);
export const EMOJI_PICKER_TOTAL_EMOJI_WIDTH =
    EMOJI_ICON_SIZE + EMOJI_GROUP_SPACING;
export const TAB_BUTTON_MARGIN = 5;
export const TEXT_GROUP_BUTTON_PADDING = 4;

export const EMOJI_ICON_SIZE_PX = `${EMOJI_ICON_SIZE}px`;
export const EMOJI_GROUP_SPACING_PX = `${EMOJI_GROUP_SPACING}px`;
export const EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX =
    `${EMOJI_PICKER_TOTAL_EMOJI_WIDTH}px`;
export const TAB_BUTTON_MARGIN_PX = `${TAB_BUTTON_MARGIN}px`;
export const TEXT_GROUP_BUTTON_PADDING_PX = `${TEXT_GROUP_BUTTON_PADDING}px`;
export const TRENDING = 'Trending';
export const RECENTLY_USED = 'Recently used';
// If more groups are added to emoji, symbol or emoticon this number will change
export const TRENDING_GROUP_ID = '30';  // TODO(b/266024083): Make this dynamic
export const NO_INTERNET_VIEW_ERROR_MSG =
    'Connect to the internet to view GIFs';
export const NO_INTERNET_SEARCH_ERROR_MSG =
    'Connect to the internet to search for GIFs';
export const SOMETHING_WENT_WRONG_ERROR_MSG = 'Something went wrong';
// 24 hours is equivalent to 86400000 milliseconds.
export const TWENTY_FOUR_HOURS = 86400000;
export const GIF_VALIDATION_DATE = 'gifValidationDate';

export const V2_5_EMOJI_CATEGORY_SIZE = 36;
export const V2_5_EMOJI_PICKER_SIDE_PADDING = 18;
export const V2_5_EMOJI_PICKER_SEARCH_SIDE_PADDING = 16;
export const V2_5_GROUP_ICON_SIZE =
    (EMOJI_PICKER_WIDTH - 2 * V2_5_EMOJI_PICKER_SIDE_PADDING) / EMOJI_PER_ROW;
export const V2_5_VISUAL_CONTENT_WIDTH =
    (EMOJI_PICKER_WIDTH - 2 * V2_5_EMOJI_PICKER_SIDE_PADDING) / 2 -
    VISUAL_CONTENT_PADDING / 2;

export const V2_5_EMOJI_SPACING =
    (EMOJI_PICKER_WIDTH - 2 * V2_5_EMOJI_PICKER_SIDE_PADDING -
     EMOJI_PER_ROW * EMOJI_ICON_SIZE) /
    (EMOJI_PER_ROW - 1);
export const V2_5_EMOJI_SPACING_PX = `${V2_5_EMOJI_SPACING}px`;
export const V2_5_EMOJI_GROUP_SIZE_PX = `${V2_5_GROUP_ICON_SIZE}px`;
export const V2_5_EMOJI_CATEGORY_SIZE_PX = `${V2_5_EMOJI_CATEGORY_SIZE}px`;
export const V2_5_EMOJI_PICKER_SEARCH_SIDE_PADDING_PX =
    `${V2_5_EMOJI_PICKER_SEARCH_SIDE_PADDING}px`;
export const V2_5_EMOJI_PICKER_SIDE_PADDING_PX =
    `${V2_5_EMOJI_PICKER_SIDE_PADDING}px`;
export const V2_5_VISUAL_CONTENT_WIDTH_PX = `${V2_5_VISUAL_CONTENT_WIDTH}px`;

export const V2_5_EMOJI_GROUP_SPACING =
    (EMOJI_PICKER_WIDTH - 2 * V2_5_EMOJI_PICKER_SIDE_PADDING -
     GROUP_PER_ROW * EMOJI_ICON_SIZE) /
    (GROUP_PER_ROW - 1);
export const V2_5_EMOJI_PICKER_TOTAL_EMOJI_WIDTH =
    EMOJI_ICON_SIZE + V2_5_EMOJI_GROUP_SPACING;

export const V2_5_EMOJI_GROUP_SPACING_PX = `${V2_5_EMOJI_GROUP_SPACING}px`;
export const V2_5_EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX =
    `${V2_5_EMOJI_PICKER_TOTAL_EMOJI_WIDTH}px`;
