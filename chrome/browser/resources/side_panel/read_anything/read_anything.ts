// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export type {SpEmptyStateElement} from '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
export {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
export {PageCallbackRouter} from '//resources/cr_components/color_change_listener/color_change_listener.mojom-webui.js';
export type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
export type {AppElement} from './app.js';
export {AppStyleUpdater} from './app_style_updater.js';
export {getWordCount, isRectMostlyVisible, isRectVisible, LOG_EMPTY_DELAY_MS, MOSTLY_VISIBLE_PERCENT, playFromSelectionTimeout, spinnerDebounceTimeout, ToolbarEvent} from './common.js';
export {ContentController, ContentListener, ContentState, ContentType, HIGHLIGHTED_LINK_CLASS} from './content_controller.js';
export {getNewIndex, isArrow, isForwardArrow, isHorizontalArrow} from './keyboard_util.js';
export type {LanguageMenuElement} from './language_menu.js';
export type {LanguageToastElement} from './language_toast.js';
export type {ColorMenuElement} from './menus/color_menu.js';
export type {HighlightMenuElement} from './menus/highlight_menu.js';
export type {LetterSpacingMenuElement} from './menus/letter_spacing_menu.js';
export type {LineSpacingMenuElement} from './menus/line_spacing_menu.js';
export {MenuStateItem} from './menus/menu_util.js';
export type {RateMenuElement} from './menus/rate_menu.js';
export type {SimpleActionMenuElement} from './menus/simple_action_menu.js';
export {MetricsBrowserProxy, MetricsBrowserProxyImpl, ReadAloudHighlightState, ReadAloudSettingsChange, ReadAnythingNewPage, ReadAnythingSettingsChange, ReadAnythingSpeechError, ReadAnythingVoiceType} from './metrics_browser_proxy.js';
export {ESTIMATED_WORDS_PER_MS, MIN_MS_TO_READ, NodeStore} from './node_store.js';
export {ReadAloudHighlighter} from './read_aloud/highlighter.js';
export {currentReadHighlightClass, Highlight, MovementGranularity, PARENT_OF_HIGHLIGHT_CLASS, PhraseHighlight, previousReadHighlightClass, SentenceHighlight, WordHighlight} from './read_aloud/movement.js';
export {getReadAloudModel, ReadAloudModelBrowserProxy, setInstance} from './read_aloud/read_aloud_model_browser_proxy.js';
export {ReadAloudNodeStore} from './read_aloud/read_aloud_node_store.js';
export {AncestorNode, AxReadAloudNode, DomReadAloudNode, ReadAloudNode, Segment} from './read_aloud/read_aloud_types.js';
export {MAX_SPEECH_LENGTH, SpeechController, SpeechListener} from './read_aloud/speech_controller.js';
export {PauseActionSource, SpeechEngineState, SpeechModel} from './read_aloud/speech_model.js';
export {getCurrentSpeechRate, isInvalidHighlightForWordHighlighting} from './read_aloud/speech_presentation_rules.js';
export {TextSegmenter} from './read_aloud/text_segmenter.js';
export {TsReadModelImpl} from './read_aloud/ts_model_impl.js';
export {V8ModelImpl} from './read_aloud/v8_model_impl.js';
export {VoiceLanguageController, VoiceLanguageListener} from './read_aloud/voice_language_controller.js';
export {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, EXTENSION_RESPONSE_TIMEOUT_MS, getFilteredVoiceList, getNotification, getVoicePackConvertedLangIfExists, mojoVoicePackStatusToVoicePackStatusEnum, NotificationType, PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from './read_aloud/voice_language_conversions.js';
export {VoiceLanguageModel} from './read_aloud/voice_language_model.js';
export {VoiceNotificationListener, VoiceNotificationManager} from './read_aloud/voice_notification_manager.js';
export type {WordBoundaryState} from './read_aloud/word_boundaries.js';
export {WordBoundaries} from './read_aloud/word_boundaries.js';
export {ReadAnythingLogger, SpeechControls, TimeFrom} from './read_anything_logger.js';
export type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';
export {IMAGES_DISABLED_ICON, IMAGES_ENABLED_ICON, IMAGES_TOGGLE_BUTTON_ID, LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, moreOptionsClass} from './read_anything_toolbar.js';
export {SelectionController} from './selection_controller.js';
export {SpeechBrowserProxy, SpeechBrowserProxyImpl} from './speech_browser_proxy.js';
export type {VoiceSelectionMenuElement} from './voice_selection_menu.js';
