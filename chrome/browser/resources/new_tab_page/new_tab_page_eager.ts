// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Eagerly load modules experimentally deferred in `new_tab_page.ts`.
// LINT.IfChange(LazyLoadedModules)
import 'chrome://new-tab-page/shared/customize_buttons/customize_buttons.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import 'chrome://resources/cr_components/search/animated_glow.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './action_chips/action_chips.js';
import './doodle_share_dialog.js';
import './ntp_composebox.js';
import './voice_search_overlay.js';

// LINT.ThenChange(
//     //chrome/browser/resources/new_tab_page/lazy_load.ts:LazyLoadedModules,
//     //chrome/test/data/webui/new_tab_page/lazy_load_test.ts:LazyLoadedModules
// )

export * from './new_tab_page.js';
