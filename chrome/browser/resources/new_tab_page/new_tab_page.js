
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file exists to make tests work. Optimizing the NTP
 * flattens most JS files into a single new_tab_page.rollup.js. Therefore, tests
 * cannot import things from individual modules anymore. This file exports the
 * things tests need.
 */

export {NtpElement} from './app.js';
export {BackgroundManager} from './background_manager.js';
export {BackgroundSelectionType, CustomizeDialogPage} from './customize_dialog_types.js';
export {recordDuration, recordLoadDuration, recordOccurence, recordPerdecage} from './metrics_utils.js';
export {ChromeCartProxy} from './modules/cart/chrome_cart_proxy.js';
export {chromeCartDescriptor} from './modules/cart/module.js';
export {chromeCartDescriptor as chromeCartV2Descriptor} from './modules/cart_v2/module.js';
export {DriveProxy} from './modules/drive/drive_module_proxy.js';
export {driveDescriptor} from './modules/drive/module.js';
export {driveDescriptor as driveV2Descriptor} from './modules/drive_v2/module.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/dummy/foo_proxy.js';
export {dummyDescriptor} from './modules/dummy/module.js';
// </if>
export {InfoDialogElement} from './modules/info_dialog.js';
export {Module, ModuleDescriptor} from './modules/module_descriptor.js';
export {ModuleHeaderElement} from './modules/module_header.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {ModulesElement} from './modules/modules.js';
// <if expr="not is_official_build">
export {photosDescriptor} from './modules/photos/module.js';
export {PhotosProxy} from './modules/photos/photos_module_proxy.js';
// </if>
export {recipeTasksDescriptor as recipeTasksV2Descriptor} from './modules/recipes_v2/module.js';
export {recipeTasksDescriptor, shoppingTasksDescriptor} from './modules/task_module/module.js';
export {TaskModuleHandlerProxy} from './modules/task_module/task_module_handler_proxy.js';
export {NewTabPageProxy} from './new_tab_page_proxy.js';
export {Command, CommandHandlerRemote} from './promo_browser_command.mojom-webui.js';
export {PromoBrowserCommandProxy} from './promo_browser_command_proxy.js';
export {RealboxBrowserProxy} from './realbox/realbox_browser_proxy.js';
export {$$, createScrollBorders, decodeString16, mojoString16} from './utils.js';
export {Action as VoiceAction, Error as VoiceError} from './voice_search_overlay.js';
export {WindowProxy} from './window_proxy.js';
