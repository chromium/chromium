
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file exists to make tests work. Optimizing the NTP
 * flattens most JS files into a single new_tab_page.rollup.js. Therefore, tests
 * cannot import things from individual modules anymore. This file exports the
 * things tests need.
 */

import './app.js';

export {BackgroundManager} from './background_manager.js';
export {BrowserProxy} from './browser_proxy.js';
export {BackgroundSelectionType} from './customize_dialog.js';
export {ImgElement} from './img.js';
// <if expr="not is_official_build">
export {dummyDescriptor} from './modules/dummy/module.js';
// </if>
export {kaleidoscopeDescriptor} from './modules/kaleidoscope/module.js';
export {ModuleDescriptor} from './modules/module_descriptor.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {shoppingTasksDescriptor} from './modules/shopping_tasks/module.js';
export {ShoppingTasksHandlerProxy} from './modules/shopping_tasks/shopping_tasks_handler_proxy.js';
export {PromoBrowserCommandProxy} from './promo_browser_command_proxy.js';
export {$$, createScrollBorders, decodeString16, mojoString16} from './utils.js';
