
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
export {BackgroundSelectionType} from './customize_dialog_types.js';
export {ImgElement} from './img.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/dummy/foo_proxy.js';
export {dummyDescriptor} from './modules/dummy/module.js';
// </if>
export {kaleidoscopeDescriptor} from './modules/kaleidoscope/module.js';
export {ModuleDescriptor} from './modules/module_descriptor.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {recipeTasksDescriptor, shoppingTasksDescriptor} from './modules/task_module/module.js';
export {TaskModuleHandlerProxy} from './modules/task_module/task_module_handler_proxy.js';
export {PromoBrowserCommandProxy} from './promo_browser_command_proxy.js';
export {$$, createScrollBorders, decodeString16, mojoString16} from './utils.js';
