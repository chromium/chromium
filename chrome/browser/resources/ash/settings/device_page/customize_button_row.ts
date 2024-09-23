// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import './customize_button_select.js';
import '../settings_shared.css.js';
import '../controls/settings_dropdown_menu.js';
import '../os_settings_icons.html.js';

import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonPressObserverReceiver} from '../mojom-webui/input_device_settings_provider.mojom-webui.js';

import {getTemplate} from './customize_button_row.html.js';
import {CustomizeButtonSelectElement} from './customize_button_select.js';
import {setDataTransferOriginIndex} from './drag_and_drop_manager.js';
import {FakeInputDeviceSettingsProvider} from './fake_input_device_settings_provider.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {ActionChoice, Button, ButtonRemapping, InputDeviceSettingsProviderInterface, MetaKey} from './input_device_settings_types.js';
import {buttonsAreEqual} from './input_device_settings_utils.js';

export interface CustomizeButtonRowElement {
  $: {
    container: HTMLDivElement,
    remappingActionDropdown: CustomizeButtonSelectElement,
    renameButton: CrIconButtonElement,
    reorderButton: CrIconButtonElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'reorder-button-direction': ReorderButtonDirectionEvent;
  }
}

/**
 * @fileoverview
 * 'keyboard-remap-key-row' contains a key with icon label and dropdown menu to
 * allow users to customize the remapped key.
 */

export type ShowRenamingDialogEvent = CustomEvent<{buttonIndex: number}>;
export type ShowKeyCustomizationDialogEvent =
    CustomEvent<{buttonIndex: number}>;
export type ReorderButtonEvent =
    CustomEvent<{originIndex: number, destinationIndex: number}>;
export type ReorderButtonDirectionEvent = CustomEvent<{direction: boolean}>;

const CustomizeButtonRowElementBase = I18nMixin(PolymerElement);

export class CustomizeButtonRowElement extends CustomizeButtonRowElementBase {
  static get is() {
    return 'customize-button-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      buttonRemappingList: {
        type: Array,
      },

      buttonRemapping_: {
        type: Object,
      },

      remappingIndex: {
        type: Number,
      },

      actionList: {
        type: Array,
      },

      /**
       * Name of the remapping.
       */
      buttonRemappingName_: {
        type: String,
        value: '',
        computed:
            'getButtonRemappingName_(buttonRemappingList.*, remappingIndex)',
      },

      /**
       * True if this element is being dragged by its handle.
       * This property is used to apply custom styling to the element when it's
       * being dragged.
       */
      isBeingDragged_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /**
       * Reference to the HTML element that was last pressed
       * on. This property is used to determine if a drag event was started
       * from this element's drag handle or elsewhere on the element..
       */
      lastMouseDownTarget_: {
        type: Object,
        value: null,
      },

      metaKey: Object,
    };
  }

  static get observers(): string[] {
    return [
      'initializeButtonRow_(buttonRemappingList.*, remappingIndex)',
    ];
  }

  buttonRemappingList: ButtonRemapping[];
  remappingIndex: number;
  actionList: ActionChoice[];
  metaKey: MetaKey = MetaKey.kSearch;
  private buttonPressObserverReceiver: ButtonPressObserverReceiver;
  private buttonRemapping_: ButtonRemapping;
  private buttonRemappingName_: string;
  private inputDeviceSettingsProvider_: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private isBeingDragged_: boolean;
  private lastMouseDownTarget_: HTMLElement|null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.observeButtonPresses();
    // Focus dropdown right away as this button was just pressed.
    this.$.remappingActionDropdown!.focus();

    this.$.reorderButton!.addEventListener(
        'keydown', this.handleKeyDownReorderButton_);
    this.addEventListener(
        'reorder-button-direction', this.onButtonReorderDirectEvent_);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.$.reorderButton!.removeEventListener(
        'keydown', this.handleKeyDownReorderButton_);
    this.removeEventListener(
        'reorder-button-direction', this.onButtonReorderDirectEvent_);
  }

  /**
   * Focuses the reordering button for this row.
   */
  focusReorderingButton(): void {
    this.$.reorderButton.focus();
  }

  override focus(): void {
    assert(this.$.remappingActionDropdown);
    this.$.remappingActionDropdown.focus();
  }

  private observeButtonPresses(): void {
    if (this.inputDeviceSettingsProvider_ instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider_.observeButtonPresses(this);
      return;
    }

    this.buttonPressObserverReceiver = new ButtonPressObserverReceiver(this);

    this.inputDeviceSettingsProvider_.observeButtonPresses(
        this.buttonPressObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /**
   * Initialize the button remapping content and set up fake pref.
   */
  private initializeButtonRow_(): void {
    if (!this.buttonRemappingList ||
        !this.buttonRemappingList[this.remappingIndex]) {
      return;
    }

    if (this.remappingIndex === 0) {
      this.$.container.classList.add('first');
    }

    this.buttonRemapping_ = this.buttonRemappingList[this.remappingIndex];
  }

  /**
   * Pops out the dialog to edit button label.
   */
  private onEditButtonLabelClicked_(): void {
    this.dispatchEvent(new CustomEvent('show-renaming-dialog', {
      bubbles: true,
      composed: true,
      detail: {buttonIndex: this.remappingIndex},
    }));
  }

  /**
   * Get the button remapping name when initializing or users updated it.
   */
  private getButtonRemappingName_(): string {
    if (!!this.buttonRemappingList &&
        !!this.buttonRemappingList[this.remappingIndex]) {
      return this.buttonRemappingList[this.remappingIndex].name;
    }
    return '';
  }

  onButtonPressed(button: Button): void {
    if (buttonsAreEqual(button, this.buttonRemapping_.button)) {
      this.$.remappingActionDropdown!.focus();
    }
  }

  private onContainerMouseDown_(event: Event): void {
    this.lastMouseDownTarget_ = event.target as HTMLElement;
  }

  private onDragStart_(event: DragEvent): void {
    const dragHandle =
        strictQuery('.move-button', this.shadowRoot, HTMLElement);
    // Check if the drag event started from the drag handle.
    if (!dragHandle.contains(this.lastMouseDownTarget_)) {
      // Drag didn't start from the handle, so don't allow the drag.
      event.preventDefault();
      return;
    }

    this.isBeingDragged_ = true;

    // Create the grey rectangle used as the drag image.
    // It's necessary to create a canvas element this way, so that we have
    // full control over the rendering of the drag image.
    const canvas = document.createElement('canvas');

    document.body.append(canvas);
    // We won't need the canvas after this function finishes, so
    // remove it from the DOM.
    setTimeout(() => canvas.remove(), 100);

    canvas.width = this.offsetWidth;
    canvas.height = this.offsetHeight;

    // Position the canvas offscreen.
    canvas.style.position = 'absolute';
    canvas.style.top = '-200px';

    // Using the canvas context, draw the grey rectangle that will be displayed
    // while the user is dragging.
    const context = canvas.getContext('2d');
    if (context) {
      // Using getComputedStyle and getPropertyValue is the only
      // way to use CSS variables with canvas painting.
      context.fillStyle = getComputedStyle(canvas).getPropertyValue(
          '--cros-sys-ripple_neutral_on_subtle');
      context.fillRect(0, 0, canvas.width, canvas.height);
      context.strokeStyle = getComputedStyle(canvas).getPropertyValue(
          '--cros-sys-highlight_shape');
      context.lineWidth = 1;
      context.strokeRect(0, 0, canvas.width, canvas.height);
    }

    if (event.dataTransfer) {
      event.dataTransfer.setDragImage(canvas, event.offsetX, event.offsetY);
      // dropEffect and effectAllowed affect the cursor that's
      // displayed during the drag.
      event.dataTransfer.dropEffect = 'move';
      event.dataTransfer.effectAllowed = 'move';
      // Set data on the event that allows the drop receiver to determine
      // the index of this row.
      setDataTransferOriginIndex(event, this.remappingIndex);
    }
  }

  private onDragEnd_(): void {
    this.isBeingDragged_ = false;
  }

  private isDropdownDisabled_(): boolean {
    return this.isBeingDragged_;
  }

  private getReorderButtonLabel_(): string {
    return this.i18n('buttonReorderingAriaLabel', this.buttonRemappingName_);
  }

  private onButtonReorderDirectEvent_(e: ReorderButtonDirectionEvent): void {
    const destinationIndex =
        this.remappingIndex + (e.detail.direction ? -1 : 1);
    this.dispatchEvent(new CustomEvent('reorder-button', {
      bubbles: true,
      composed: true,
      detail: {originIndex: this.remappingIndex, destinationIndex},
    }));
  }

  private handleKeyDownReorderButton_(e: KeyboardEvent): void {
    if (!e.ctrlKey) {
      return;
    }

    const isArrowUp = e.key === 'ArrowUp';
    const isArrowDown = e.key === 'ArrowDown';
    if (isArrowUp || isArrowDown) {
      this.dispatchEvent(new CustomEvent('reorder-button-direction', {
        bubbles: true,
        composed: true,
        detail: {direction: isArrowUp ? true : false},
      }));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonRowElement.is]: CustomizeButtonRowElement;
  }
}

customElements.define(CustomizeButtonRowElement.is, CustomizeButtonRowElement);
