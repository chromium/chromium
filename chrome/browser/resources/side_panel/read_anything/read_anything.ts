// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export type {SpEmptyStateElement} from '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
export {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
export {PageCallbackRouter} from '//resources/cr_components/color_change_listener/color_change_listener.mojom-webui.js';
export type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
export type {AppElement, WordBoundaryState} from './app.js';
export {currentReadHighlightClass, MAX_SPEECH_LENGTH_FOR_REMOTE_VOICES, MAX_SPEECH_LENGTH_FOR_WORD_BOUNDARIES, PauseActionSource, previousReadHighlightClass, WordBoundaryMode} from './app.js';
export {AppStyleUpdater} from './app_style_updater.js';
export {getCurrentSpeechRate, playFromSelectionTimeout, spinnerDebounceTimeout, ToolbarEvent} from './common.js';
export {getNewIndex, isArrow, isForwardArrow, isHorizontalArrow} from './keyboard_util.js';
export type {LanguageMenuElement} from './language_menu.js';
export type {LanguageToastElement} from './language_toast.js';
export type {ColorMenuElement} from './menus/color_menu.js';
export type {HighlightMenuElement} from './menus/highlight_menu.js';
export type {LetterSpacingMenuElement} from './menus/letter_spacing_menu.js';
export type {LineSpacingMenuElement} from './menus/line_spacing_menu.js';
export {MenuStateItem} from './menus/menu_util.js';
export type {SimpleActionMenuElement} from './menus/simple_action_menu.js';
export {MetricsBrowserProxy, MetricsBrowserProxyImpl, ReadAloudHighlightState, ReadAloudSettingsChange, ReadAnythingNewPage, ReadAnythingSettingsChange, ReadAnythingSpeechError, ReadAnythingVoiceType} from './metrics_browser_proxy.js';
export {ReadAnythingLogger, SpeechControls, TimeFrom} from './read_anything_logger.js';
export type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
export {IMAGES_DISABLED_ICON, IMAGES_ENABLED_ICON, IMAGES_TOGGLE_BUTTON_ID, LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, moreOptionsClass} from './read_anything_toolbar.js';
export {SpeechBrowserProxy, SpeechBrowserProxyImpl} from './speech_browser_proxy.js';
export {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, getFilteredVoiceList, getNotification, mojoVoicePackStatusToVoicePackStatusEnum, NotificationType, PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from './voice_language_util.js';
export {VoiceNotificationListener, VoiceNotificationManager} from './voice_notification_manager.js';
export type {VoiceSelectionMenuElement} from './voice_selection_menu.js';
