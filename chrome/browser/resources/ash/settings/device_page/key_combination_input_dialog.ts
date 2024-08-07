// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome" >
import 'chrome://resources/ash/common/internal/ash_internal_icons.html.js';
// </if>

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ShortcutInputElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonRemapping, KeyEvent, MetaKey, ShortcutInputProviderInterface} from './input_device_settings_types.js';
import {keyEventsAreEqual} from './input_device_settings_utils.js';
import {getTemplate} from './key_combination_input_dialog.html.js';
import {getShortcutInputProvider} from './shortcut_input_mojo_interface_provider.js';

/**
 * @fileoverview
 * 'key-combination-input-dialog' is a dialog that pops up after clicking the
 * 'Key combination' choice in the dropdown menu to allow users to input a
 * combination of keyboard keys as a button remapping action.
 */

export interface KeyCombinationInputDialogElement {
  $: {
    keyCombinationInputDialog: CrDialogElement,
    shortcutInput: ShortcutInputElement,
  };
}

export type ShortcutInputCompleteEvent = CustomEvent<{keyEvent: KeyEvent}>;
export type ShortcutInputCaptureStateEvent = CustomEvent<{capturing: boolean}>;

declare global {
  interface HTMLElementEventMap {
    'shortcut-input-event': ShortcutInputCompleteEvent;
  }
}

const KeyCombinationInputDialogElementBase = I18nMixin(PolymerElement);

export class KeyCombinationInputDialogElement extends
    KeyCombinationInputDialogElementBase {
  static get is() {
    return 'key-combination-input-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      buttonRemappingList: {
        type: Array,
      },

      remappingIndex: {
        type: Number,
      },

      buttonRemapping_: {
        type: Object,
      },

      isOpen: {
        type: Boolean,
        value: false,
      },

      isCapturing: {
        type: Boolean,
      },

      inputKeyEvent: {
        type: Object,
      },

      metaKey: Object,
    };
  }

  static get observers(): string[] {
    return [
      'initializeDialog(buttonRemappingList.*, remappingIndex)',
    ];
  }

  buttonRemappingList: ButtonRemapping[];
  remappingIndex: number;
  isOpen: boolean;
  shortcutInput: ShortcutInputElement;
  inputKeyEvent: KeyEvent|undefined;
  isCapturing: boolean = false;
  metaKey: MetaKey = MetaKey.kSearch;
  private buttonRemapping_: ButtonRemapping;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback(): void {
    super.connectedCallback();
    this.eventTracker_.add(
        this, 'shortcut-input-event', this.onShortcutInputEvent_);
    this.eventTracker_.add(
        this, 'shortcut-input-capture-state', this.onShortcutInputUpdate_);
    // Set window as the eventTarget to exclude blur event from shortcut-input.
    this.eventTracker_.add(window, 'blur', this.onBlur_.bind(this));
    this.shortcutInput = this.$.shortcutInput;
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  /**
   * Initialize the button remapping content and set up fake pref.
   */
  private initializeDialog(): void {
    if (!this.buttonRemappingList ||
        !this.buttonRemappingList[this.remappingIndex]) {
      return;
    }
    this.buttonRemapping_ = this.buttonRemappingList[this.remappingIndex];
  }

  showModal(): void {
    this.initializeDialog();
    const keyCombinationInputDialog = this.$.keyCombinationInputDialog;
    keyCombinationInputDialog.showModal();
    this.isOpen = keyCombinationInputDialog.open;
    this.shortcutInput.reset();
    this.shortcutInput.startObserving();
  }

  close(): void {
    const keyCombinationInputDialog = this.$.keyCombinationInputDialog;
    keyCombinationInputDialog.close();
    this.isOpen = keyCombinationInputDialog.open;

    this.shortcutInput.reset();
    this.shortcutInput.stopObserving();
  }

  getShortcutProvider(): ShortcutInputProviderInterface {
    return getShortcutInputProvider();
  }

  private onBlur_(): void {
    if (this.isCapturing && !this.inputKeyEvent && this.isOpen) {
      this.close();
    }
  }

  private cancelDialogClicked_(): void {
    this.close();
  }

  private saveDialogClicked_(): void {
    if (!this.inputKeyEvent) {
      return;
    }
    const prevKeyEvent: KeyEvent|undefined =
        this.buttonRemapping_.remappingAction?.keyEvent;
    if (!prevKeyEvent ||
        !keyEventsAreEqual(this.inputKeyEvent, prevKeyEvent!)) {
      this.set(
          `buttonRemappingList.${this.remappingIndex}`,
          this.getUpdatedButtonRemapping_());
      this.dispatchEvent(new CustomEvent('button-remapping-changed', {
        bubbles: true,
        composed: true,
      }));
    }
    this.close();
  }

  /**
   * @returns Button remapping with updated remapping action based on
   * users' key combination input.
   */
  private getUpdatedButtonRemapping_(): ButtonRemapping {
    return {
      ...this.buttonRemapping_,
      remappingAction: {
        keyEvent: this.inputKeyEvent,
      },
    };
  }

  /**
   * Listens for ShortcutInputCompleteEvent to store users' input keyEvent.
   */
  private onShortcutInputEvent_(e: ShortcutInputCompleteEvent): void {
    this.inputKeyEvent = e.detail.keyEvent;
    this.shortcutInput.stopObserving();
  }

  private onShortcutInputUpdate_(e: ShortcutInputCaptureStateEvent): void {
    this.isCapturing = e.detail.capturing;
  }

  private onEditButtonClicked_(): void {
    this.inputKeyEvent = undefined;
    this.shortcutInput.reset();
    this.shortcutInput.startObserving();
  }

  private shouldDisableSaveButton_(): boolean {
    return this.inputKeyEvent === undefined;
  }

  private shouldShowEditButton_(): boolean {
    return !this.isCapturing;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [KeyCombinationInputDialogElement.is]: KeyCombinationInputDialogElement;
  }
}

customElements.define(
    KeyCombinationInputDialogElement.is, KeyCombinationInputDialogElement);
