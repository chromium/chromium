// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
export {PageCallbackRouter} from '//resources/cr_components/color_change_listener/color_change_listener.mojom-webui.js';
export type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
export type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
export type {ReadAnythingElement, WordBoundaryState} from './app.js';
export {currentReadHighlightClass, PauseActionSource, previousReadHighlightClass, WordBoundaryMode} from './app.js';
export {defaultFontName, playFromSelectionTimeout} from './common.js';
export type {LanguageMenuElement} from './language_menu.js';
export {LANGUAGE_TOGGLE_EVENT} from './language_menu.js';
export type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
export {FONT_EVENT, FONT_SIZE_EVENT, HIGHLIGHT_TOGGLE_EVENT, LETTER_SPACING_EVENT, LINE_SPACING_EVENT, LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, LINKS_EVENT, moreOptionsClass, NEXT_GRANULARITY_EVENT, PLAY_PAUSE_EVENT, PREVIOUS_GRANULARITY_EVENT, RATE_EVENT, THEME_EVENT} from './read_anything_toolbar.js';
export {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, mojoVoicePackStatusToVoicePackStatusEnum, PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode, VoicePackStatus} from './voice_language_util.js';
export type {VoiceSelectionMenuElement} from './voice_selection_menu.js';
export {PLAY_PREVIEW_EVENT} from './voice_selection_menu.js';
