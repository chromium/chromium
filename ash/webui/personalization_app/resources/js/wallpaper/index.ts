// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_error_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_images_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_input_query_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_recent_wallpapers_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_suggestions_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_template_query_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_templates_element.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_introduction_dialog_element.js';
import './google_photos_albums_element.js';
import './google_photos_collection_element.js';
import './google_photos_photos_by_album_id_element.js';
import './google_photos_photos_element.js';
import './google_photos_zero_state_element.js';
import './local_images_element.js';
import './time_of_day_wallpaper_dialog_element.js';
import './wallpaper_collections_element.js';
import './wallpaper_error_element.js';
import './wallpaper_fullscreen_element.js';
import './wallpaper_images_element.js';
import './wallpaper_preview_element.js';
import './wallpaper_selected_element.js';
import './wallpaper_subpage_element.js';

function reload(): void {
  window.location.reload();
}
// Reload when online, in case any images are not loaded.
window.addEventListener('online', reload);
