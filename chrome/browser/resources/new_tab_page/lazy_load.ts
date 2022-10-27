// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file holds dependencies that are deliberately excluded
 * from the main app.js module, to force them to load after other app.js
 * dependencies. This is done to improve performance of initial rendering of
 * core elements of the landing page, by delaying loading of non-core
 * elements (either not visible by default or not as performance critical).
 */

import './customize_dialog.js';
import './middle_slot_promo.js';
import './voice_search_overlay.js';
import './modules/module_descriptors.js';
import 'chrome://resources/cr_components/most_visited/most_visited.js';

export {CustomizeBackgroundsElement} from './customize_backgrounds.js';
export {CustomizeDialogElement} from './customize_dialog.js';
export {CustomizeModulesElement} from './customize_modules.js';
export {CustomizeShortcutsElement} from './customize_shortcuts.js';
export {LensErrorType, LensFormElement, LensSubmitType} from './lens_form.js';
export {LensUploadDialogAction, LensUploadDialogElement, LensUploadDialogError} from './lens_upload_dialog.js';
export {MiddleSlotPromoElement, PromoDismissAction} from './middle_slot_promo.js';
export {ChromeCartProxy} from './modules/cart/chrome_cart_proxy.js';
export {DiscountConsentCard, DiscountConsentVariation} from './modules/cart/discount_consent_card.js';
export {DiscountConsentDialog} from './modules/cart/discount_consent_dialog.js';
export {chromeCartDescriptor, ChromeCartModuleElement} from './modules/cart/module.js';
export {chromeCartDescriptor as chromeCartV2Descriptor, ChromeCartModuleElement as ChromeCartV2ModuleElement} from './modules/cart_v2/module.js';
export {DriveProxy} from './modules/drive/drive_module_proxy.js';
export {driveDescriptor, DriveModuleElement} from './modules/drive/module.js';
export {driveDescriptor as driveV2Descriptor, DriveModuleElement as DriveV2ModuleElement} from './modules/drive_v2/module.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/dummy_v2/foo_proxy.js';
export {DummyModuleElement, dummyV2Descriptor} from './modules/dummy_v2/module.js';
// </if>
export {FeedProxy} from './modules/feed/feed_module_proxy.js';
export {feedDescriptor, FeedModuleElement, feedV2Descriptor} from './modules/feed/module.js';
export {InfoDialogElement} from './modules/info_dialog.js';
export {InitializeModuleCallback, Module, ModuleDescriptor, ModuleDescriptorV2, ModuleHeight} from './modules/module_descriptor.js';
export {counterfactualLoad} from './modules/module_descriptors.js';
export {ModuleHeaderElement} from './modules/module_header.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {ModuleWrapperElement} from './modules/module_wrapper.js';
export {DisableModuleEvent, DismissModuleEvent, ModulesElement} from './modules/modules.js';
export {photosDescriptor, PhotosModuleElement} from './modules/photos/module.js';
export {PhotosProxy} from './modules/photos/photos_module_proxy.js';
export {RecipesModuleElement, recipeTasksDescriptor} from './modules/recipes/module.js';
export {RecipesHandlerProxy} from './modules/recipes/recipes_handler_proxy.js';
export {RecipesModuleElement as RecipesModuleElementV2, recipeTasksDescriptor as recipeTasksV2Descriptor} from './modules/recipes_v2/module.js';
export {VoiceSearchOverlayElement} from './voice_search_overlay.js';
