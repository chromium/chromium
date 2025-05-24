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

import './middle_slot_promo.js';
import './voice_search_overlay.js';
import './modules/module_descriptors.js';
import 'chrome://resources/cr_components/most_visited/most_visited.js';

export {CustomizeButtonsElement} from 'chrome://new-tab-page/shared/customize_buttons/customize_buttons.js';
export {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
export {LensErrorType, LensFormElement, LensSubmitType} from './lens_form.js';
export {LensUploadDialogAction, LensUploadDialogElement, LensUploadDialogError} from './lens_upload_dialog.js';
export {MiddleSlotPromoElement, PromoDismissAction} from './middle_slot_promo.js';
export {InfoDialogElement} from './modules/info_dialog.js';
export {ParentTrustedDocumentProxy} from './modules/microsoft_auth_frame_connector.js';
export {InitializeModuleCallback, Module, ModuleDescriptor} from './modules/module_descriptor.js';
export {counterfactualLoad} from './modules/module_descriptors.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {ModuleInstance, ModuleWrapperElement} from './modules/module_wrapper.js';
export {microsoftAuthModuleDescriptor, MicrosoftAuthModuleElement} from './modules/v2/authentication/microsoft_auth_module.js';
export {MicrosoftAuthProxyImpl} from './modules/v2/authentication/microsoft_auth_module_proxy.js';
export {CalendarElement} from './modules/v2/calendar/calendar.js';
export {CalendarEventElement} from './modules/v2/calendar/calendar_event.js';
export {CalendarAction} from './modules/v2/calendar/common.js';
export {googleCalendarDescriptor, GoogleCalendarModuleElement} from './modules/v2/calendar/google_calendar_module.js';
export {GoogleCalendarProxyImpl} from './modules/v2/calendar/google_calendar_proxy.js';
export {outlookCalendarDescriptor, OutlookCalendarModuleElement} from './modules/v2/calendar/outlook_calendar_module.js';
export {OutlookCalendarProxyImpl} from './modules/v2/calendar/outlook_calendar_proxy.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/v2/dummy/foo_proxy.js';
export {dummyV2Descriptor, ModuleElement as DummyModuleElement} from './modules/v2/dummy/module.js';
// </if>
export {DriveModuleElement as DriveModuleV2Element, driveModuleV2Descriptor} from './modules/v2/file_suggestion/drive_module.js';
export {FileProxy} from './modules/v2/file_suggestion/file_module_proxy.js';
export {FileSuggestionElement} from './modules/v2/file_suggestion/file_suggestion.js';
export {microsoftFilesModuleDescriptor, MicrosoftFilesModuleElement} from './modules/v2/file_suggestion/microsoft_files_module.js';
export {MicrosoftFilesProxyImpl} from './modules/v2/file_suggestion/microsoft_files_proxy.js';
export {ModuleHeaderElement as ModuleHeaderElementV2} from './modules/v2/module_header.js';
export {DisableModuleEvent, DismissModuleElementEvent, DismissModuleInstanceEvent, ModulesElement, NamedWidth, SUPPORTED_MODULE_WIDTHS} from './modules/v2/modules.js';
export {ModuleElement as MostRelevantTabResumptionModuleElement, mostRelevantTabResumptionDescriptor} from './modules/v2/most_relevant_tab_resumption/module.js';
export {MostRelevantTabResumptionProxyImpl} from './modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_proxy.js';
export {VoiceSearchOverlayElement} from './voice_search_overlay.js';
