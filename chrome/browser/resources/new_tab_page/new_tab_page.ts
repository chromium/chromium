
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file exists to make tests work. Optimizing the NTP
 * flattens most JS files into a single new_tab_page.rollup.js. Therefore, tests
 * cannot import things from individual modules anymore. This file exports the
 * things tests need.
 */

export {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
export {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
export {NtpElement} from './app.js';
export {BackgroundManager} from './background_manager.js';
export {CustomizeDialogPage} from './customize_dialog_types.js';
export {recordDuration, recordLoadDuration, recordOccurence, recordPerdecage} from './metrics_utils.js';
export {ChromeCartProxy} from './modules/cart/chrome_cart_proxy.js';
export {chromeCartDescriptor} from './modules/cart/module.js';
export {chromeCartDescriptor as chromeCartV2Descriptor} from './modules/cart_v2/module.js';
export {DriveProxy} from './modules/drive/drive_module_proxy.js';
export {driveDescriptor, DriveModuleElement} from './modules/drive/module.js';
export {driveDescriptor as driveV2Descriptor, DriveModuleElement as DriveV2ModuleElement} from './modules/drive_v2/module.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/dummy_v2/foo_proxy.js';
export {DummyModuleElement, dummyV2Descriptor} from './modules/dummy_v2/module.js';
// </if>
export {InfoDialogElement} from './modules/info_dialog.js';
export {InitializeModuleCallback, Module, ModuleDescriptor, ModuleDescriptorV2, ModuleHeight} from './modules/module_descriptor.js';
export {ModuleHeaderElement} from './modules/module_header.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {ModuleWrapperElement} from './modules/module_wrapper.js';
export {DisableModuleEvent, DismissModuleEvent, ModulesElement} from './modules/modules.js';
export {photosDescriptor} from './modules/photos/module.js';
export {PhotosProxy} from './modules/photos/photos_module_proxy.js';
export {RecipeModuleElement, recipeTasksDescriptor as recipeTasksV2Descriptor} from './modules/recipes_v2/module.js';
export {recipeTasksDescriptor, shoppingTasksDescriptor} from './modules/task_module/module.js';
export {TaskModuleHandlerProxy} from './modules/task_module/task_module_handler_proxy.js';
export {NewTabPageProxy} from './new_tab_page_proxy.js';
export {RealboxElement} from './realbox/realbox.js';
export {RealboxBrowserProxy} from './realbox/realbox_browser_proxy.js';
export {RealboxIconElement} from './realbox/realbox_icon.js';
export {RealboxMatchElement} from './realbox/realbox_match.js';
export {$$, createScrollBorders, decodeString16, mojoString16} from './utils.js';
export {Action as VoiceAction, Error as VoiceError} from './voice_search_overlay.js';
export {WindowProxy} from './window_proxy.js';
