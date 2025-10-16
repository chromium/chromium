// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '/strings.m.js';

import {getInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import type {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import type {KeyboardDiagramElement} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {BottomLeftLayout as DiagramBottomLeftLayout, BottomRightLayout as DiagramBottomRightLayout, MechanicalLayout as DiagramMechanicalLayout, NumberPadLayout as DiagramNumberPadLayout, PhysicalLayout as DiagramPhysicalLayout, SplitModifierTopRowKey as DiagramSplitModifierTopRowKey, TopRightKey as DiagramTopRightKey, TopRowKey as DiagramTopRowKey} from 'chrome://resources/ash/common/keyboard_diagram.js';
import {KeyboardKeyState} from 'chrome://resources/ash/common/keyboard_key.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {KeyboardInfo} from './input.mojom-webui.js';
import {BottomLeftLayout, BottomRightLayout, MechanicalLayout, NumberPadPresence, NumpadLayout, PhysicalLayout, TopRightKey, TopRowKey} from './input.mojom-webui.js';
import type {InputDataProviderInterface, KeyEvent} from './input_data_provider.mojom-webui.js';
import {KeyboardObserverReceiver, KeyEventType} from './input_data_provider.mojom-webui.js';
import {getTemplate} from './keyboard_tester.html.js';
import {getInputDataProvider, getSystemDataProvider} from './mojo_interface_provider.js';
import type {SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';

export interface KeyboardTesterElement {
  $: {
    dialog: CrDialogElement,
    lostFocusToast: CrToastElement,
  };
}

export type AnnounceTextEvent = CustomEvent<{text: string}>;

declare global {
  interface HTMLElementEventMap {
    'announce-text': AnnounceTextEvent;
  }
}

export interface KeyboardDiagramTopRowKey {
  icon?: string;
  ariaNameI18n?: string;
  text?: string;
}

/**
 * @fileoverview
 * 'keyboard-tester' displays a tester UI for a keyboard.
 */

/**
 * Map from Mojo TopRowKey constants to keyboard diagram top row key
 * definitions.
 */
const topRowKeyMap: {[index: number]: KeyboardDiagramTopRowKey} = {
  [TopRowKey.kNone]: DiagramTopRowKey['kNone'],
  [TopRowKey.kBack]: DiagramTopRowKey['kBack'],
  [TopRowKey.kForward]: DiagramTopRowKey['kForward'],
  [TopRowKey.kRefresh]: DiagramTopRowKey['kRefresh'],
  [TopRowKey.kFullscreen]: DiagramTopRowKey['kFullscreen'],
  [TopRowKey.kOverview]: DiagramTopRowKey['kOverview'],
  [TopRowKey.kScreenshot]: DiagramTopRowKey['kScreenshot'],
  [TopRowKey.kScreenBrightnessDown]: DiagramTopRowKey['kScreenBrightnessDown'],
  [TopRowKey.kScreenBrightnessUp]: DiagramTopRowKey['kScreenBrightnessUp'],
  [TopRowKey.kPrivacyScreenToggle]: DiagramTopRowKey['kPrivacyScreenToggle'],
  [TopRowKey.kMicrophoneMute]: DiagramTopRowKey['kMicrophoneMute'],
  [TopRowKey.kVolumeMute]: DiagramTopRowKey['kVolumeMute'],
  [TopRowKey.kVolumeDown]: DiagramTopRowKey['kVolumeDown'],
  [TopRowKey.kVolumeUp]: DiagramTopRowKey['kVolumeUp'],
  [TopRowKey.kKeyboardBacklightToggle]:
      DiagramTopRowKey['kKeyboardBacklightToggle'],
  [TopRowKey.kKeyboardBacklightDown]:
      DiagramTopRowKey['kKeyboardBacklightDown'],
  [TopRowKey.kKeyboardBacklightUp]: DiagramTopRowKey['kKeyboardBacklightUp'],
  [TopRowKey.kNextTrack]: DiagramTopRowKey['kNextTrack'],
  [TopRowKey.kPreviousTrack]: DiagramTopRowKey['kPreviousTrack'],
  [TopRowKey.kPlayPause]: DiagramTopRowKey['kPlayPause'],
  [TopRowKey.kScreenMirror]: DiagramTopRowKey['kScreenMirror'],
  [TopRowKey.kAccessibility]: DiagramTopRowKey['kAccessibility'],
  [TopRowKey.kDictation]: DiagramTopRowKey['kDictation'],
  [TopRowKey.kDelete]: DiagramTopRowKey['kDelete'],
  [TopRowKey.kUnknown]: DiagramTopRowKey['kUnknown'],
};

/**
 * Map from Mojo TopRowKey constants to split modifier specific
 * keyboard diagram top row key definitions.
 */
const splitModifierTopRowKeyMap: {[index: number]: KeyboardDiagramTopRowKey} = {
  [TopRowKey.kOverview]: DiagramSplitModifierTopRowKey['kOverview'],
};

/** Maps top-right key evdev codes to the corresponding DiagramTopRightKey. */
const topRightKeyByCode: Map<number, DiagramTopRightKey> = new Map([
  [116, DiagramTopRightKey.POWER],
  [142, DiagramTopRightKey.LOCK],
  [579, DiagramTopRightKey.CONTROL_PANEL],
]);

/** Evdev codes for keys that always appear in the number pad area. */
const numberPadCodes: Set<number> = new Set([
  55,   // KEY_KPASTERISK
  71,   // KEY_KP7
  72,   // KEY_KP8
  73,   // KEY_KP9
  74,   // KEY_KPMINUS
  75,   // KEY_KP4
  76,   // KEY_KP5
  77,   // KEY_KP6
  78,   // KEY_KPPLUS
  79,   // KEY_KP1
  80,   // KEY_KP2
  81,   // KEY_KP3
  82,   // KEY_KP0
  83,   // KEY_KPDOT
  96,   // KEY_KPENTER
  98,   // KEY_KPSLASH
  102,  // KEY_HOME
  107,  // KEY_END
]);

/**
 * Evdev codes for keys that appear in the number pad area on standard ChromeOS
 * keyboards, but not on Dell Enterprise ones.
 */
const standardNumberPadCodes: Set<number> = new Set([
  104,  // KEY_PAGEUP
  109,  // KEY_PAGEDOWN
  111,  // KEY_DELETE
]);

const acerSplitModifierKeyboardWithNumpadBoards = [
  'ruke',
  'jubileum',
  'ruken',
];

const DISPLAY_TOAST_INDEFINITELY_MS = 0;
const TOAST_LINGER_MS = 1000;

const KeyboardTesterElementBase = I18nMixin(PolymerElement);

export class KeyboardTesterElement extends KeyboardTesterElementBase {
  static get is(): string {
    return 'keyboard-tester';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The keyboard being tested, or null if none is being tested at the
       * moment.
       */
      keyboard: {type: Object},

      shouldDisplayDiagram: {
        type: Boolean,
        computed: 'computeShouldDisplayDiagram(keyboard)',
      },

      diagramMechanicalLayout: {
        type: String,
        computed: 'computeDiagramMechanicalLayout(keyboard)',
      },

      diagramPhysicalLayout: {
        type: String,
        computed: 'computeDiagramPhysicalLayout(keyboard)',
      },

      diagramTopRightKey: {
        type: String,
        computed: 'computeDiagramTopRightKey(keyboard)',
      },

      showNumberPad: {
        type: Boolean,
        computed: 'computeShowNumberPad(keyboard)',
      },

      showAssistantKey: {
        type: Boolean,
        computed: 'computeShowAssistantKey(keyboard)',
      },

      topRowKeys: {
        type: Array,
        computed: 'computeTopRowKeys(keyboard)',
      },

      bottomLeftLayout: {
        type: String,
        computed: 'computeBottomLeftLayout(keyboard)',
      },

      bottomRightLayout: {
        type: String,
        computed: 'computeBottomRightLayout(keyboard)',
      },

      numberPadLayout: {
        type: String,
        computed: 'computeNumberPadLayout(keyboard)',
      },

      isLoggedIn: {
        type: Boolean,
        value: loadTimeData.getBoolean('isLoggedIn'),
      },

      lostFocusToastLingerMs: {
        type: Number,
        value: DISPLAY_TOAST_INDEFINITELY_MS,
      },

    };
  }

  constructor() {
    // For acer split modifier keyboard w/ numpad, the top row is different
    // from standard reference design, therefore need a physical layout
    // exception.
    super();
    this.systemDataProvider.getSystemInfo().then(
        (result: {systemInfo: SystemInfo}) => {
          this.onSystemInfoReceived(result.systemInfo);
        });
  }

  private onSystemInfoReceived(systemInfo: SystemInfo): void {
    this.boardName = systemInfo.boardName;
  }

  keyboard: KeyboardInfo;
  isLoggedIn: boolean;
  protected diagramTopRightKey: DiagramTopRightKey|null;
  private lostFocusToastLingerMs: number;
  private shouldDisplayDiagram: boolean;
  private diagramMechanicalLayout: DiagramMechanicalLayout|null;
  private diagramPhysicalLayout: DiagramPhysicalLayout|null;
  private showNumberPad: boolean;
  private showAssistantKey: boolean;
  private boardName: string = '';
  private topRowKeys: KeyboardDiagramTopRowKey[];
  private bottomLeftLayout: string;
  private bottomRightLayout: string;
  private numberPadLayout: string;
  private receiver: KeyboardObserverReceiver|null = null;
  private inputDataProvider: InputDataProviderInterface =
      getInputDataProvider();
  private systemDataProvider: SystemDataProviderInterface =
      getSystemDataProvider();
  private eventTracker: EventTracker = new EventTracker();

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  /**
   * Event callback for 'announce-text' which is triggered from keyboard-key.
   * Event will contain text to announce to screen readers.
   */
  private announceTextHandler = (e: AnnounceTextEvent): void => {
    assert(e.detail.text);
    e.stopPropagation();
    getInstance(this.$.dialog.getNative()).announce(e.detail.text);
  };

  private computeShouldDisplayDiagram(keyboard?: KeyboardInfo): boolean {
    if (!keyboard) {
      return false;
    }
    return keyboard.physicalLayout !== PhysicalLayout.kUnknown &&
        keyboard.mechanicalLayout !== MechanicalLayout.kUnknown;
    // Number pad presence can be unknown, as we can adapt on the fly if we get
    // a number pad event we weren't expecting.
  }

  private computeDiagramMechanicalLayout(keyboardInfo?: KeyboardInfo):
      DiagramMechanicalLayout|null {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [MechanicalLayout.kUnmappedEnumField]: null,
      [MechanicalLayout.kUnknown]: null,
      [MechanicalLayout.kAnsi]: DiagramMechanicalLayout.ANSI,
      [MechanicalLayout.kIso]: DiagramMechanicalLayout.ISO,
      [MechanicalLayout.kJis]: DiagramMechanicalLayout.JIS,
    }[keyboardInfo.mechanicalLayout];
  }

  private computeDiagramPhysicalLayout(keyboardInfo?: KeyboardInfo):
      DiagramPhysicalLayout|null {
    if (!keyboardInfo) {
      return null;
    }

    if (acerSplitModifierKeyboardWithNumpadBoards.includes(this.boardName)) {
      return DiagramPhysicalLayout.ACER_SPLIT_MODIFIER_WITH_NUMPAD;
    }

    if (keyboardInfo.bottomLeftLayout !== BottomLeftLayout.kUnknown &&
        keyboardInfo.bottomRightLayout !== BottomRightLayout.kUnknown) {
      return DiagramPhysicalLayout.SPLIT_MODIFIER;
    }

    return {
      [PhysicalLayout.kUnmappedEnumField]: null,
      [PhysicalLayout.kUnknown]: null,
      [PhysicalLayout.kChromeOS]: DiagramPhysicalLayout.CHROME_OS,
      [PhysicalLayout.kChromeOSDellEnterpriseWilco]:
          DiagramPhysicalLayout.CHROME_OS_DELL_ENTERPRISE_WILCO,
      [PhysicalLayout.kChromeOSDellEnterpriseDrallion]:
          DiagramPhysicalLayout.CHROME_OS_DELL_ENTERPRISE_DRALLION,
    }[keyboardInfo.physicalLayout];
  }

  private computeDiagramTopRightKey(keyboardInfo?: KeyboardInfo):
      DiagramTopRightKey|null {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [TopRightKey.kUnmappedEnumField]: null,
      [TopRightKey.kUnknown]: null,
      [TopRightKey.kPower]: DiagramTopRightKey.POWER,
      [TopRightKey.kLock]: DiagramTopRightKey.LOCK,
      [TopRightKey.kControlPanel]: DiagramTopRightKey.CONTROL_PANEL,
    }[keyboardInfo.topRightKey];
  }

  private computeShowNumberPad(keyboard?: KeyboardInfo): boolean {
    return !!keyboard &&
        (keyboard.numberPadPresent === NumberPadPresence.kPresent ||
         !!keyboard.numpadLayout);
  }

  private computeShowAssistantKey(keyboard?: KeyboardInfo): boolean {
    const physicalLayout = this.computeDiagramPhysicalLayout(keyboard);
    return !!keyboard && keyboard.hasAssistantKey &&
        physicalLayout !== DiagramPhysicalLayout.SPLIT_MODIFIER &&
        physicalLayout !==
        DiagramPhysicalLayout.ACER_SPLIT_MODIFIER_WITH_NUMPAD;
  }

  private computeTopRowKeys(keyboard?: KeyboardInfo):
      KeyboardDiagramTopRowKey[] {
    if (!keyboard) {
      return [];
    }
    if (this.computeDiagramPhysicalLayout(keyboard) ===
        DiagramPhysicalLayout.SPLIT_MODIFIER) {
      return keyboard.topRowKeys.map(
          (keyId: TopRowKey) =>
              splitModifierTopRowKeyMap[keyId] ?? topRowKeyMap[keyId]);
    }
    return keyboard.topRowKeys.map((keyId: TopRowKey) => topRowKeyMap[keyId]);
  }

  private computeBottomLeftLayout(keyboardInfo?: KeyboardInfo):
      DiagramBottomLeftLayout|null {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [BottomLeftLayout.kUnknown]: null,
      [BottomLeftLayout.kBottomLeft3Keys]: DiagramBottomLeftLayout.THREE_KEYS,
      [BottomLeftLayout.kBottomLeft4Keys]: DiagramBottomLeftLayout.FOUR_KEYS,
    }[keyboardInfo.bottomLeftLayout];
  }

  private computeBottomRightLayout(keyboardInfo?: KeyboardInfo):
      DiagramBottomRightLayout|null {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [BottomRightLayout.kUnknown]: null,
      [BottomRightLayout.kBottomRight2Keys]: DiagramBottomRightLayout.TWO_KEYS,
      [BottomRightLayout.kBottomRight3Keys]:
          DiagramBottomRightLayout.THREE_KEYS,
      [BottomRightLayout.kBottomRight4Keys]: DiagramBottomRightLayout.FOUR_KEYS,
    }[keyboardInfo.bottomRightLayout];
  }

  private computeNumberPadLayout(keyboardInfo?: KeyboardInfo):
      DiagramNumberPadLayout|null {
    if (!keyboardInfo) {
      return null;
    }
    return {
      [NumpadLayout.kUnknown]: null,
      [NumpadLayout.kNumpad3Column]: DiagramNumberPadLayout.THREE_COLUMN,
      [NumpadLayout.kNumpad4Column]: DiagramNumberPadLayout.FOUR_COLUMN,
    }[keyboardInfo.numpadLayout];
  }

  protected getDescriptionLabel(): string {
    return this.i18n('keyboardTesterInstruction');
  }

  protected getShortcutInstructionLabel(): TrustedHTML {
    return this.i18nAdvanced(
        'keyboardTesterShortcutInstruction', {attrs: ['id']});
  }

  private addEventListeners(): void {
    this.eventTracker.add(
        document, 'keydown', (e: KeyboardEvent) => this.onKeyPress(e));
    this.eventTracker.add(
        document, 'keyup', (e: KeyboardEvent) => this.onKeyPress(e));
    this.eventTracker.add(
        document, 'announce-text',
        (e: AnnounceTextEvent) => this.announceTextHandler(e));
  }

  /** Shows the tester's dialog. */
  show(): void {
    assert(this.inputDataProvider);
    this.receiver = new KeyboardObserverReceiver(this);
    this.inputDataProvider.observeKeyEvents(
        this.keyboard.id, this.receiver.$.bindNewPipeAndPassRemote());
    this.addEventListeners();
    const title: HTMLElement|null =
        this.shadowRoot!.querySelector('div[slot="title"]');
    this.$.dialog.getNative().removeAttribute('aria-describedby');
    this.$.dialog.showModal();
    title?.focus();
  }

  // Prevent the default behavior for keydown/keyup only when the keyboard
  // tester dialog is opened.
  onKeyPress(e: KeyboardEvent): void {
    if (!this.isOpen()) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();

    // If we receive alt + esc we should close the tester.
    if (e.type === 'keydown' && e.altKey && e.key === 'Escape') {
      this.close();
    }
  }

  /**
   * Returns whether the tester is currently open.
   */
  isOpen(): boolean {
    return this.$.dialog.open;
  }

  close(): void {
    if (this.shouldDisplayDiagram) {
      const diagram: KeyboardDiagramElement|null =
          this.shadowRoot!.querySelector('#diagram');
      assert(diagram);
      diagram.resetAllKeys();
    }
    this.$.dialog.close();

    const url = new URL(window.location.href);
    url.searchParams.delete('showDefaultKeyboardTester');
    history.pushState(null, '', url);
  }

  handleClose(): void {
    this.eventTracker.removeAll();
    if (this.receiver) {
      this.receiver.$.close();
    }
  }

  /**
   * Returns whether a key is part of the number pad on this keyboard layout.
   */
  private isNumberPadKey(evdevCode: number): boolean {
    // Some keys that are on the number pad on standard ChromeOS keyboards are
    // elsewhere on Dell Enterprise keyboards, so we should only check them if
    // we know this is a standard layout.
    if (this.keyboard.physicalLayout === PhysicalLayout.kChromeOS &&
        standardNumberPadCodes.has(evdevCode)) {
      return true;
    }

    return numberPadCodes.has(evdevCode);
  }

  /**
   * Implements KeyboardObserver.OnKeyEvent.
   * @param {!KeyEvent} keyEvent
   */
  onKeyEvent(keyEvent: KeyEvent): void {
    const diagram: KeyboardDiagramElement|null =
        this.shadowRoot!.querySelector('#diagram');
    assert(diagram);
    const state = keyEvent.type === KeyEventType.kPress ?
        KeyboardKeyState.PRESSED :
        KeyboardKeyState.TESTED;
    if (keyEvent.topRowPosition !== -1 &&
        keyEvent.topRowPosition < this.keyboard.topRowKeys.length) {
      diagram.setTopRowKeyState(keyEvent.topRowPosition, state);
    } else {
      // We can't be sure that the top right key reported over Mojo is correct,
      // so we need to fix it if we see a key event that suggests it's wrong.
      if (topRightKeyByCode.has(keyEvent.keyCode) &&
          diagram.topRightKey !== topRightKeyByCode.get(keyEvent.keyCode)) {
        const newValue =
            topRightKeyByCode.get(keyEvent.keyCode) as DiagramTopRightKey;
        diagram.topRightKey = newValue;
      }

      // Some Chromebooks (at least the Lenovo ThinkPad C13 Yoga a.k.a.
      // Morphius) report F13 instead of SLEEP when Lock is pressed.
      if (keyEvent.keyCode === 183 /* KEY_F13 */) {
        keyEvent.keyCode = 142 /* KEY_SLEEP */;
      }

      // There may be Chromebooks where hasNumberPad is incorrect, so if we see
      // any number pad key codes we need to adapt on-the-fly.
      if (!diagram.showNumberPad && this.isNumberPadKey(keyEvent.keyCode)) {
        diagram.showNumberPad = true;
      }

      diagram.setKeyState(keyEvent.keyCode, state);
    }
  }

  /**
   * Implements KeyboardObserver.OnKeyEventsPaused.
   */
  onKeyEventsPaused(): void {
    const diagram: KeyboardDiagramElement|null =
        this.shadowRoot!.querySelector('#diagram');
    assert(diagram);
    diagram.clearPressedKeys();
    this.lostFocusToastLingerMs = DISPLAY_TOAST_INDEFINITELY_MS;
    this.$.lostFocusToast.show();
  }

  /**
   * Implements KeyboardObserver.OnKeyEventsResumed.
   */
  onKeyEventsResumed(): void {
    if (this.isOpen()) {
      this.$.dialog.focus();
    }

    // Show focus lost toast for 1 second after regaining focus.
    this.lostFocusToastLingerMs = TOAST_LINGER_MS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'keyboard-tester': KeyboardTesterElement;
  }
}

customElements.define(KeyboardTesterElement.is, KeyboardTesterElement);
