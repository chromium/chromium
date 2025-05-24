// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export {EmojiPickerApp} from './app.js';
/**
 * With optimize_webui, the generated JS files are bundled into single file
 * `chrome://emoji-picker/emoji_picker.js`. These exports are
 * necessary so they can be imported in tests.
 */
export {EMOJI_PICKER_TOTAL_EMOJI_WIDTH, GIF_VALIDATION_DATE, TRENDING, TRENDING_GROUP_ID} from './constants.js';
export {EmojiButton} from './emoji_button.js';
export {EmojiGroupComponent} from './emoji_group.js';
export {Category} from './emoji_picker.mojom-webui.js';
export {EmojiPickerApiProxy} from './emoji_picker_api_proxy.js';
export {EmojiSearch} from './emoji_search.js';
export {EMOJI_IMG_BUTTON_CLICK, EMOJI_PICKER_READY, EMOJI_TEXT_BUTTON_CLICK, EMOJI_VARIANTS_SHOWN} from './events.js';
export {EmojiPrefixSearch} from './prefix_search.js';
export {Trie} from './structs/trie.js';
export {PaginatedGifResponses, Status} from './tenor_types.mojom-webui.js';
export {EmojiGroupElement, GifSubcategoryData, VisualContent} from './types.js';
