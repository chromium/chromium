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

import './action_chips/action_chips.js';
import './action_chips/action_chips_proxy.js';
import './lens_upload_dialog.js';
import './middle_slot_promo.js';
import './modules/module_descriptors.js';
import './modules/modules.js';
import './ntp_promo/individual_promos.js';
import './ntp_promo/ntp_promo_proxy.js';
import './ntp_promo/setup_list_module_wrapper.js';
import './voice_search_overlay.js';
import 'chrome://resources/cr_components/most_visited/most_visited.js';
import 'chrome://resources/cr_components/composebox/composebox.js';

export {CustomizeButtonsElement} from 'chrome://new-tab-page/shared/customize_buttons/customize_buttons.js';
export {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
export {ComposeboxElement, VoiceSearchAction} from 'chrome://resources/cr_components/composebox/composebox.js';
export {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
export {ContextualEntrypointAndCarouselElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_carousel.js';
export {ErrorScrimElement} from 'chrome://resources/cr_components/composebox/error_scrim.js';
export {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
export {ComposeboxFileThumbnailElement} from 'chrome://resources/cr_components/composebox/file_thumbnail.js';
export {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
export {PluralStringProxyImpl as NTPPluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
export {ActionChipsElement, ActionChipsRetrievalState} from './action_chips/action_chips.js';
export {ActionChipsApiProxyImpl} from './action_chips/action_chips_proxy.js';
export {LensErrorType, LensFormElement, LensSubmitType} from './lens_form.js';
export {LensUploadDialogAction, LensUploadDialogElement, LensUploadDialogError} from './lens_upload_dialog.js';
export {MiddleSlotPromoElement, PromoDismissAction} from './middle_slot_promo.js';
export {microsoftAuthModuleDescriptor, MicrosoftAuthModuleElement} from './modules/authentication/microsoft_auth_module.js';
export {MicrosoftAuthProxyImpl} from './modules/authentication/microsoft_auth_module_proxy.js';
export {CalendarElement} from './modules/calendar/calendar.js';
export {CalendarEventElement} from './modules/calendar/calendar_event.js';
export {CalendarAction} from './modules/calendar/common.js';
export {googleCalendarDescriptor, GoogleCalendarModuleElement} from './modules/calendar/google_calendar_module.js';
export {GoogleCalendarProxyImpl} from './modules/calendar/google_calendar_proxy.js';
export {outlookCalendarDescriptor, OutlookCalendarModuleElement} from './modules/calendar/outlook_calendar_module.js';
export {OutlookCalendarProxyImpl} from './modules/calendar/outlook_calendar_proxy.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/dummy/foo_proxy.js';
export {dummyV2Descriptor, ModuleElement as DummyModuleElement} from './modules/dummy/module.js';
// </if>
export {driveModuleDescriptor, DriveModuleElement as DriveModuleV2Element} from './modules/file_suggestion/drive_module.js';
export {FileProxy} from './modules/file_suggestion/file_module_proxy.js';
export {FileSuggestionElement} from './modules/file_suggestion/file_suggestion.js';
export {microsoftFilesModuleDescriptor, MicrosoftFilesModuleElement} from './modules/file_suggestion/microsoft_files_module.js';
export {MicrosoftFilesProxyImpl} from './modules/file_suggestion/microsoft_files_proxy.js';
export {InfoDialogElement} from './modules/info_dialog.js';
export {ParentTrustedDocumentProxy} from './modules/microsoft_auth_frame_connector.js';
export {InitializeModuleCallback, Module, ModuleDescriptor} from './modules/module_descriptor.js';
export {counterfactualLoad} from './modules/module_descriptors.js';
export {ModuleHeaderElement as ModuleHeaderElementV2} from './modules/module_header.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {ModuleInstance, ModuleWrapperElement} from './modules/module_wrapper.js';
export {DisableModuleEvent, DismissModuleElementEvent, DismissModuleInstanceEvent, ModulesElement, NamedWidth, SUPPORTED_MODULE_WIDTHS} from './modules/modules.js';
export {ModuleElement as MostRelevantTabResumptionModuleElement, mostRelevantTabResumptionDescriptor} from './modules/most_relevant_tab_resumption/module.js';
export {MostRelevantTabResumptionProxyImpl} from './modules/most_relevant_tab_resumption/most_relevant_tab_resumption_proxy.js';
export {IconContainerElement} from './modules/tab_groups/icon_container.js';
export {COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_DOT_PREFIX, COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_PREFIX, colorIdToString, ModuleElement as TabGroupsModuleElement, tabGroupsDescriptor} from './modules/tab_groups/module.js';
export {TabGroupsProxyImpl} from './modules/tab_groups/tab_groups_proxy.js';
export {IndividualPromosElement} from './ntp_promo/individual_promos.js';
export {NtpPromoProxy, NtpPromoProxyImpl} from './ntp_promo/ntp_promo_proxy.js';
export {SetupListElement} from './ntp_promo/setup_list.js';
export {SetupListItemElement} from './ntp_promo/setup_list_item.js';
export {SetupListModuleWrapperElement} from './ntp_promo/setup_list_module_wrapper.js';
export {VoiceSearchOverlayElement} from './voice_search_overlay.js';
