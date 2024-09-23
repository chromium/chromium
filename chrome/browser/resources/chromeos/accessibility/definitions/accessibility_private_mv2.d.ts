// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.accessibilityPrivate API
 * Partially generated from:
 * chrome/common/extensions/api/accessibility_private.json This file exists
 * because MV3 supports promises and MV2 does not.
 * TODO(b/260590502): Delete this after MV3 migration.
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/accessibility_private.json -g ts_definitions` to
 * regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace accessibilityPrivate {

      export const IS_DEFAULT_EVENT_SOURCE_TOUCH: number;

      export interface AlertInfo {
        message: string;
      }

      export interface ScreenRect {
        left: number;
        top: number;
        width: number;
        height: number;
      }

      export interface ScreenPoint {
        x: number;
        y: number;
      }

      export enum Gesture {
        CLICK = 'click',
        SWIPE_LEFT1 = 'swipeLeft1',
        SWIPE_UP1 = 'swipeUp1',
        SWIPE_RIGHT1 = 'swipeRight1',
        SWIPE_DOWN1 = 'swipeDown1',
        SWIPE_LEFT2 = 'swipeLeft2',
        SWIPE_UP2 = 'swipeUp2',
        SWIPE_RIGHT2 = 'swipeRight2',
        SWIPE_DOWN2 = 'swipeDown2',
        SWIPE_LEFT3 = 'swipeLeft3',
        SWIPE_UP3 = 'swipeUp3',
        SWIPE_RIGHT3 = 'swipeRight3',
        SWIPE_DOWN3 = 'swipeDown3',
        SWIPE_LEFT4 = 'swipeLeft4',
        SWIPE_UP4 = 'swipeUp4',
        SWIPE_RIGHT4 = 'swipeRight4',
        SWIPE_DOWN4 = 'swipeDown4',
        TAP2 = 'tap2',
        TAP3 = 'tap3',
        TAP4 = 'tap4',
        TOUCH_EXPLORE = 'touchExplore',
      }

      export enum MagnifierCommand {
        MOVE_STOP = 'moveStop',
        MOVE_UP = 'moveUp',
        MOVE_DOWN = 'moveDown',
        MOVE_LEFT = 'moveLeft',
        MOVE_RIGHT = 'moveRight',
      }

      export enum SwitchAccessCommand {
        SELECT = 'select',
        NEXT = 'next',
        PREVIOUS = 'previous',
      }

      export enum PointScanState {
        START = 'start',
        STOP = 'stop',
      }

      export enum SwitchAccessBubble {
        BACK_BUTTON = 'backButton',
        MENU = 'menu',
      }

      export interface PointScanPoint {
        x: number;
        y: number;
      }

      export enum SwitchAccessMenuAction {
        COPY = 'copy',
        CUT = 'cut',
        DECREMENT = 'decrement',
        DICTATION = 'dictation',
        DRILL_DOWN = 'drillDown',
        END_TEXT_SELECTION = 'endTextSelection',
        INCREMENT = 'increment',
        ITEM_SCAN = 'itemScan',
        JUMP_TO_BEGINNING_OF_TEXT = 'jumpToBeginningOfText',
        JUMP_TO_END_OF_TEXT = 'jumpToEndOfText',
        KEYBOARD = 'keyboard',
        LEFT_CLICK = 'leftClick',
        MOVE_BACKWARD_ONE_CHAR_OF_TEXT = 'moveBackwardOneCharOfText',
        MOVE_BACKWARD_ONE_WORD_OF_TEXT = 'moveBackwardOneWordOfText',
        MOVE_CURSOR = 'moveCursor',
        MOVE_DOWN_ONE_LINE_OF_TEXT = 'moveDownOneLineOfText',
        MOVE_FORWARD_ONE_CHAR_OF_TEXT = 'moveForwardOneCharOfText',
        MOVE_FORWARD_ONE_WORD_OF_TEXT = 'moveForwardOneWordOfText',
        MOVE_UP_ONE_LINE_OF_TEXT = 'moveUpOneLineOfText',
        PASTE = 'paste',
        POINT_SCAN = 'pointScan',
        RIGHT_CLICK = 'rightClick',
        SCROLL_DOWN = 'scrollDown',
        SCROLL_LEFT = 'scrollLeft',
        SCROLL_RIGHT = 'scrollRight',
        SCROLL_UP = 'scrollUp',
        SELECT = 'select',
        SETTINGS = 'settings',
        START_TEXT_SELECTION = 'startTextSelection',
      }

      export enum SyntheticKeyboardEventType {
        KEYUP = 'keyup',
        KEYDOWN = 'keydown',
      }

      export interface SyntheticKeyboardModifiers {
        ctrl?: boolean;
        alt?: boolean;
        search?: boolean;
        shift?: boolean;
      }

      export interface SyntheticKeyboardEvent {
        type: SyntheticKeyboardEventType;
        keyCode: number;
        modifiers?: SyntheticKeyboardModifiers;
      }

      export enum SyntheticMouseEventType {
        PRESS = 'press',
        RELEASE = 'release',
        DRAG = 'drag',
        MOVE = 'move',
        ENTER = 'enter',
        EXIT = 'exit',
      }

      export enum SyntheticMouseEventButton {
        LEFT = 'left',
        MIDDLE = 'middle',
        RIGHT = 'right',
        BACK = 'back',
        FOWARD = 'foward',
      }

      export interface SyntheticMouseEvent {
        type: SyntheticMouseEventType;
        x: number;
        y: number;
        touchAccessibility?: boolean;
        mouseButton?: SyntheticMouseEventButton;
        isDoubleClick?: boolean;
      }

      export enum SelectToSpeakState {
        SELECTING = 'selecting',
        SPEAKING = 'speaking',
        INACTIVE = 'inactive',
      }

      export enum FocusType {
        GLOW = 'glow',
        SOLID = 'solid',
        DASHED = 'dashed',
      }

      export enum FocusRingStackingOrder {
        ABOVE_ACCESSIBILITY_BUBBLES = 'aboveAccessibilityBubbles',
        BELOW_ACCESSIBILITY_BUBBLES = 'belowAccessibilityBubbles',
      }

      export enum AssistiveTechnologyType {
        CHROME_VOX = 'chromeVox',
        SELECT_TO_SPEAK = 'selectToSpeak',
        SWITCH_ACCESS = 'switchAccess',
        AUTO_CLICK = 'autoClick',
        MAGNIFIER = 'magnifier',
        DICTATION = 'dictation',
      }

      export interface FocusRingInfo {
        rects: ScreenRect[];
        type: FocusType;
        color: string;
        secondaryColor?: string;
        backgroundColor?: string;
        stackingOrder?: FocusRingStackingOrder;
        id?: string;
      }

      export enum AcceleratorAction {
        FOCUS_PREVIOUS_PANE = 'focusPreviousPane',
        FOCUS_NEXT_PANE = 'focusNextPane',
      }

      export enum AccessibilityFeature {
        GOOGLE_TTS_LANGUAGE_PACKS = 'googleTtsLanguagePacks',
        DICTATION_CONTEXT_CHECKING = 'dictationContextChecking',
        FACE_GAZE = 'faceGaze',
        GOOGLE_TTS_HIGH_QUALITY_VOICES = 'googleTtsHighQualityVoices',
        FACE_GAZE_GRAVITY_WELLS = 'faceGazeGravityWells',
      }

      export enum SelectToSpeakPanelAction {
        PREVIOUS_PARAGRAPH = 'previousParagraph',
        PREVIOUS_SENTENCE = 'previousSentence',
        PAUSE = 'pause',
        RESUME = 'resume',
        NEXT_SENTENCE = 'nextSentence',
        NEXT_PARAGRAPH = 'nextParagraph',
        EXIT = 'exit',
        CHANGE_SPEED = 'changeSpeed',
      }

      export enum SetNativeChromeVoxResponse {
        SUCCESS = 'success',
        TALKBACK_NOT_INSTALLED = 'talkbackNotInstalled',
        WINDOW_NOT_FOUND = 'windowNotFound',
        FAILURE = 'failure',
        NEED_DEPRECATION_CONFIRMATION = 'needDeprecationConfirmation',
      }

      export enum DictationBubbleIconType {
        HIDDEN = 'hidden',
        STANDBY = 'standby',
        MACRO_SUCCESS = 'macroSuccess',
        MACRO_FAIL = 'macroFail',
      }

      export enum DictationBubbleHintType {
        TRY_SAYING = 'trySaying',
        TYPE = 'type',
        DELETE = 'delete',
        SELECT_ALL = 'selectAll',
        UNDO = 'undo',
        HELP = 'help',
        UNSELECT = 'unselect',
        COPY = 'copy',
      }

      export interface DictationBubbleProperties {
        visible: boolean;
        icon: DictationBubbleIconType;
        text?: string;
        hints?: DictationBubbleHintType[];
      }

      export enum ToastType {
        DICTATION_NO_FOCUSED_TEXT_FIELD = 'dictationNoFocusedTextField',
        DICTATION_MIC_MUTED = 'dictationMicMuted',
      }

      export enum DlcType {
        TTS_BN_BD = 'ttsBnBd',
        TTS_CS_CZ = 'ttsCsCz',
        TTS_DA_DK = 'ttsDaDk',
        TTS_DE_DE = 'ttsDeDe',
        TTS_EL_GR = 'ttsElGr',
        TTS_EN_AU = 'ttsEnAu',
        TTS_EN_GB = 'ttsEnGb',
        TTS_EN_US = 'ttsEnUs',
        TTS_ES_ES = 'ttsEsEs',
        TTS_ES_US = 'ttsEsUs',
        TTS_FI_FI = 'ttsFiFi',
        TTS_FIL_PH = 'ttsFilPh',
        TTS_FR_FR = 'ttsFrFr',
        TTS_HI_IN = 'ttsHiIn',
        TTS_HU_HU = 'ttsHuHu',
        TTS_ID_ID = 'ttsIdId',
        TTS_IT_IT = 'ttsItIt',
        TTS_JA_JP = 'ttsJaJp',
        TTS_KM_KH = 'ttsKmKh',
        TTS_KO_KR = 'ttsKoKr',
        TTS_NB_NO = 'ttsNbNo',
        TTS_NE_NP = 'ttsNeNp',
        TTS_NL_NL = 'ttsNlNl',
        TTS_PL_PL = 'ttsPlPl',
        TTS_PT_BR = 'ttsPtBr',
        TTS_PT_PT = 'ttsPtPt',
        TTS_SI_LK = 'ttsSiLk',
        TTS_SK_SK = 'ttsSkSk',
        TTS_SV_SE = 'ttsSvSe',
        TTS_TH_TH = 'ttsThTh',
        TTS_TR_TR = 'ttsTrTr',
        TTS_UK_UA = 'ttsUkUa',
        TTS_VI_VN = 'ttsViVn',
        TTS_YUE_HK = 'ttsYueHk',
      }

      export enum TtsVariant {
        LITE = 'lite',
        STANDARD = 'standard',
      }

      export interface PumpkinData {
        js_pumpkin_tagger_bin_js: ArrayBuffer;
        tagger_wasm_main_js: ArrayBuffer;
        tagger_wasm_main_wasm: ArrayBuffer;
        en_us_action_config_binarypb: ArrayBuffer;
        en_us_pumpkin_config_binarypb: ArrayBuffer;
        fr_fr_action_config_binarypb: ArrayBuffer;
        fr_fr_pumpkin_config_binarypb: ArrayBuffer;
        it_it_action_config_binarypb: ArrayBuffer;
        it_it_pumpkin_config_binarypb: ArrayBuffer;
        de_de_action_config_binarypb: ArrayBuffer;
        de_de_pumpkin_config_binarypb: ArrayBuffer;
        es_es_action_config_binarypb: ArrayBuffer;
        es_es_pumpkin_config_binarypb: ArrayBuffer;
      }

      export interface FaceGazeAssets {
        model: ArrayBuffer;
        wasm: ArrayBuffer;
      }

      export enum ScrollDirection {
        UP = 'up',
        DOWN = 'down',
        LEFT = 'left',
        RIGHT = 'right',
      }

      export enum FacialGesture {
        BROW_INNER_UP = 'browInnerUp',
        BROWS_DOWN = 'browsDown',
        EYE_SQUINT_LEFT = 'eyeSquintLeft',
        EYE_SQUINT_RIGHT = 'eyeSquintRight',
        EYES_BLINK = 'eyesBlink',
        EYES_LOOK_DOWN = 'eyesLookDown',
        EYES_LOOK_LEFT = 'eyesLookLeft',
        EYES_LOOK_RIGHT = 'eyesLookRight',
        EYES_LOOK_UP = 'eyesLookUp',
        JAW_LEFT = 'jawLeft',
        JAW_OPEN = 'jawOpen',
        JAW_RIGHT = 'jawRight',
        MOUTH_FUNNEL = 'mouthFunnel',
        MOUTH_LEFT = 'mouthLeft',
        MOUTH_PUCKER = 'mouthPucker',
        MOUTH_RIGHT = 'mouthRight',
        MOUTH_SMILE = 'mouthSmile',
        MOUTH_UPPER_UP = 'mouthUpperUp',
      }

      export interface GestureInfo {
        gesture: FacialGesture;
        confidence: number;
      }

      export function getDisplayNameForLocale(
          localeCodeToTranslate: string, displayLocaleCode: string): string;

      type GetBatteryDescriptionCallback = (description: string) => void;
      export function getBatteryDescription(
          callback: GetBatteryDescriptionCallback): void;

      type InstallFaceGazeAssetsCallback = (assets: FaceGazeAssets) => void;
      export function installFaceGazeAssets(
          callback: InstallFaceGazeAssetsCallback): void;

      type InstallPumpkinForDictationCallback = (data: PumpkinData) => void;
      export function installPumpkinForDictation(
          callback: InstallPumpkinForDictationCallback): void;

      export function setNativeAccessibilityEnabled(enabled: boolean): void;

      export function setFocusRings(
          focusRings: FocusRingInfo[], atType: AssistiveTechnologyType): void;

      export function setHighlights(rects: ScreenRect[], color: string): void;

      export function setKeyboardListener(enabled: boolean, capture: boolean):
          void;

      export function setChromeVoxFocus(bounds: ScreenRect): void;

      export function setSelectToSpeakFocus(bounds: ScreenRect): void;

      export function darkenScreen(darken: boolean): void;

      export function forwardKeyEventsToSwitchAccess(shouldForward: boolean):
          void;

      export function updateSwitchAccessBubble(
          bubble: SwitchAccessBubble, show: boolean, anchor?: ScreenRect,
          actions?: SwitchAccessMenuAction[]): void;

      export function setPointScanState(state: PointScanState): void;

      type SetNativeChromeVoxArcSupportForCurrentAppCallback =
          (response: SetNativeChromeVoxResponse) => void;
      export function setNativeChromeVoxArcSupportForCurrentApp(
          enabled: boolean,
          callback: SetNativeChromeVoxArcSupportForCurrentAppCallback): void;

      export function sendSyntheticKeyEvent(
          keyEvent: SyntheticKeyboardEvent, useRewriters?: boolean): void;

      export function enableMouseEvents(enabled: boolean): void;

      export function setCursorPosition(point: ScreenPoint): void;

      export function sendSyntheticMouseEvent(mouseEvent: SyntheticMouseEvent):
          void;

      export function setSelectToSpeakState(state: SelectToSpeakState): void;

      export function clipboardCopyInActiveLacrosGoogleDoc(url: string): void;

      export function handleScrollableBoundsForPointFound(rect: ScreenRect):
          void;

      export function moveMagnifierToRect(rect: ScreenRect): void;

      export function magnifierCenterOnPoint(point: ScreenPoint): void;

      export function toggleDictation(): void;

      export function setVirtualKeyboardVisible(isVisible: boolean): void;

      export function openSettingsSubpage(subpage: string): void;

      export function performAcceleratorAction(acceleratorAction:
                                                   AcceleratorAction): void;

      type IsFeatureEnabledCallback = (enabled: boolean) => void;
      export function isFeatureEnabled(
          feature: AccessibilityFeature,
          callback: IsFeatureEnabledCallback): void;

      export function updateSelectToSpeakPanel(
          show: boolean, anchor?: ScreenRect, isPaused?: boolean,
          speed?: number): void;

      type ShowConfirmationDialogCallback = (confirmed: boolean) => void;
      export function showConfirmationDialog(
          title: string, description: string, cancelName: string|undefined,
          callback: ShowConfirmationDialogCallback): void;

      type GetLocalizedDomKeyStringForKeyCodeCallback = (result:
                                                             string) => void;
      export function getLocalizedDomKeyStringForKeyCode(
          keyCode: number,
          callback: GetLocalizedDomKeyStringForKeyCodeCallback): void;

      export function updateDictationBubble(
          properties: DictationBubbleProperties): void;

      export function silenceSpokenFeedback(): void;

      type GetDlcContentsCallback = (contents: ArrayBuffer) => void;
      export function getDlcContents(
          dlc: DlcType, callback: GetDlcContentsCallback): void;

      export function getTtsDlcContents(
          dlc: DlcType, variant: TtsVariant,
          callback: GetDlcContentsCallback): void;

      type IsLacrosPrimaryCallback = (result: boolean) => void;
      export function isLacrosPrimary(callback: IsLacrosPrimaryCallback): void;

      export function getDisplayBounds(
          callback: (screens: ScreenRect[]) => void): void;

      export function showToast(type: ToastType): void;

      export function scrollAtPoint(
          target: ScreenPoint, direction: ScrollDirection): void;

      export function sendGestureInfoToSettings(gestureInfo: GestureInfo[]):
          void;

      export function updateFaceGazeBubble(text: string): void;

      export const onIntroduceChromeVox: ChromeEvent<() => void>;

      export const onChromeVoxFocusChanged:
          ChromeEvent<(bounds: ScreenRect) => void>;

      export const onAccessibilityGesture:
          ChromeEvent<(gesture: Gesture, x: number, y: number) => void>;

      export const onSelectToSpeakContextMenuClicked: ChromeEvent<() => void>;

      export const onSelectToSpeakFocusChanged:
          ChromeEvent<(bounds: ScreenRect) => void>;

      export const onSelectToSpeakStateChangeRequested: ChromeEvent<() => void>;

      export const onSelectToSpeakKeysPressedChanged:
          ChromeEvent<(keyCodes: number[]) => void>;

      export const onSelectToSpeakMouseChanged: ChromeEvent<
          (type: SyntheticMouseEventType, x: number, y: number) => void>;

      export const onSelectToSpeakPanelAction: ChromeEvent<
          (action: SelectToSpeakPanelAction, value?: number) => void>;

      export const onSwitchAccessCommand:
          ChromeEvent<(command: SwitchAccessCommand) => void>;

      export const onPointScanSet: ChromeEvent<(point: PointScanPoint) => void>;

      export const onMagnifierCommand:
          ChromeEvent<(command: MagnifierCommand) => void>;

      export const onAnnounceForAccessibility:
          ChromeEvent<(announceText: string[]) => void>;

      export const onScrollableBoundsForPointRequested:
          ChromeEvent<(x: number, y: number) => void>;

      export const onMagnifierBoundsChanged:
          ChromeEvent<(magnifierBounds: ScreenRect) => void>;

      export const onCustomSpokenFeedbackToggled:
          ChromeEvent<(enabled: boolean) => void>;

      export const onShowChromeVoxTutorial: ChromeEvent<() => void>;

      export const onToggleDictation: ChromeEvent<(activated: boolean) => void>;

      export const onToggleGestureInfoForSettings:
          ChromeEvent<(enabled: boolean) => void>;
    }
  }
}
