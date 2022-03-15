// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview the main entry point for the Personalization SWA. This imports
 * all of the necessary global modules and polymer elements to bootstrap the
 * page.
 */

import '/strings.m.js';
import './ambient/album_list_element.js';
import './ambient/albums_subpage_element.js';
import './ambient/animation_theme_item_element.js';
import './ambient/animation_theme_list_element.js';
import './ambient/art_album_dialog_element.js';
import './ambient/ambient_preview_element.js';
import './ambient/ambient_subpage_element.js';
import './ambient/ambient_weather_element.js';
import './ambient/toggle_row_element.js';
import './ambient/topic_source_item_element.js';
import './ambient/topic_source_list_element.js';
import './ambient/zero_state_element.js';
import './personalization_router_element.js';
import './personalization_test_api.js';
import './personalization_toast_element.js';
import './personalization_breadcrumb_element.js';
import './personalization_main_element.js';
import './personalization_theme_element.js';
import './user/avatar_camera_element.js';
import './user/avatar_list_element.js';
import './user/user_preview_element.js';
import './user/user_subpage_element.js';
import './wallpaper/index.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {emptyState} from './personalization_state.js';
import {PersonalizationStore} from './personalization_store.js';

PersonalizationStore.getInstance().init(emptyState());
document.title = loadTimeData.getBoolean('isPersonalizationHubEnabled') ?
    loadTimeData.getString('personalizationTitle') :
    loadTimeData.getString('wallpaperLabel');
