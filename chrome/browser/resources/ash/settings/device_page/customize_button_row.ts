// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';
import '/shared/settings/controls/settings_dropdown_menu.js';
import '../os_settings_icons.html.js';

import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonPressObserverReceiver} from '../mojom-webui/input_device_settings_provider.mojom-webui.js';

import {getTemplate} from './customize_button_row.html.js';
import {setDataTransferOriginIndex} from './drag_and_drop_manager.js';
import {FakeInputDeviceSettingsProvider} from './fake_input_device_settings_provider.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {ActionChoice, Button, ButtonRemapping, InputDeviceSettingsProviderInterface, KeyEvent, RemappingAction} from './input_device_settings_types.js';
import {buttonsAreEqual} from './input_device_settings_utils.js';

const NO_REMAPPING_OPTION_LABEL = 'none';
const KEY_COMBINATION_OPTION_LABEL = 'key combination';
const OPEN_DIALOG_OPTION_LABEL = 'open key combination dialog';
const ACCELERATOR_ACTION_PREFIX = 'acceleratorAction';
const STATICS_SHORTCUT_ACTION_PREFIX = 'staticShortcutAction';

/**
 * Bit mask of modifiers.
 * Ordering is according to UX, but values match EventFlags in
 * ui/events/event_constants.h.
 */
enum Modifier {
  NONE = 0,
  CONTROL = 1 << 2,
  SHIFT = 1 << 1,
  ALT = 1 << 3,
  META = 1 << 4,
}

/**
 * Map the modifier keys to the bit value. Currently the modifiers only
 * contains the following four.
 */
const modifierBitMaskToString: Map<number, string> = new Map([
  [Modifier.CONTROL, 'ctrl'],
  [Modifier.SHIFT, 'shift'],
  [Modifier.ALT, 'alt'],
  [Modifier.META, 'meta'],
]);

function concateKeyString(firstStr: string, secondStr: string): string {
  return firstStr.length === 0 ? secondStr : firstStr.concat(` + ${secondStr}`);
}

export interface CustomizeButtonRowElement {
  $: {
    remappingActionDropdown: HTMLSelectElement,
  };
}

/**
 * Converts a keyEvent to a string representing all the modifiers and the vkey.
 */
function getKeyCombinationLabel(keyEvent: KeyEvent): string {
  let combinationLabel = '';
  modifierBitMaskToString.forEach((modifierName: string, bitValue: number) => {
    if ((keyEvent.modifiers & bitValue) !== 0) {
      combinationLabel = concateKeyString(combinationLabel, modifierName);
    }
  });
  if (keyEvent.keyDisplay !== undefined && keyEvent.keyDisplay.length !== 0) {
    combinationLabel = concateKeyString(combinationLabel, keyEvent.keyDisplay);
  }
  return combinationLabel;
}

/**
 * @fileoverview
 * 'keyboard-remap-key-row' contains a key with icon label and dropdown menu to
 * allow users to customize the remapped key.
 */

export type ShowRenamingDialogEvent = CustomEvent<{buttonIndex: number}>;
export type ShowKeyCustomizationDialogEvent =
    CustomEvent<{buttonIndex: number}>;

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

      buttonMapTargets_: {
        type: Object,
      },

      remappingIndex: {
        type: Number,
      },

      fakePref_: {
        type: Object,
        value() {
          return {
            key: 'fakeCustomizeKeyPref',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: NO_REMAPPING_OPTION_LABEL,
          };
        },
      },

      actionList: {
        type: Array,
        observer: 'setUpButtonMapTargets_',
      },

      removeTopBorder: {
        type: Boolean,
        reflectToAttribute: true,
      },

      keyCombinationLabel_: {
        type: String,
      },

      /**
       * The value of the "None" item in dropdown menu.
       */
      noRemappingOptionValue_: {
        type: String,
        value: NO_REMAPPING_OPTION_LABEL,
        readOnly: true,
      },

      /**
       * The value of the "Key combination" item in dropdown menu.
       */
      keyCombinationOptionValue_: {
        type: String,
        value: KEY_COMBINATION_OPTION_LABEL,
        readOnly: true,
      },

      openDialogOptionValue_: {
        type: String,
        value: OPEN_DIALOG_OPTION_LABEL,
        readOnly: true,
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
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(fakePref_.*)',
      'initializeCustomizeKey(buttonRemappingList.*, remappingIndex)',
    ];
  }

  buttonRemappingList: ButtonRemapping[];
  remappingIndex: number;
  actionList: ActionChoice[];
  private buttonPressObserverReceiver: ButtonPressObserverReceiver;
  private buttonRemapping_: ButtonRemapping;
  private buttonMapTargets_: DropdownMenuOptionList;
  private fakePref_: chrome.settingsPrivate.PrefObject;
  private noRemappingOptionValue_: string;
  private keyCombinationOptionValue_: string;
  private openDialogOptionValue_: string;
  private keyCombinationLabel_: string;
  private buttonRemappingName_: string;
  private isInitialized_: boolean;
  private prevChoice_: string;
  private inputDeviceSettingsProvider_: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private isBeingDragged_: boolean;
  private lastMouseDownTarget_: HTMLElement|null;

  override connectedCallback(): void {
    super.connectedCallback();
    this.observeButtonPresses();
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
   * Initialize dropdown menu.
   */
  private initializeDropdown_(
      originalAction: string, dropdown: HTMLSelectElement): void {
    // Initialize fakePref with originalAction.
    this.set('fakePref_.value', originalAction);

    // Initialize dropdown menu selection to match the
    // originalAction.
    const option = this.buttonMapTargets_.find((dropdownItem) => {
      return dropdownItem.value === originalAction;
    });

    microTask.run(() => {
      dropdown.value =
          option === undefined ? NO_REMAPPING_OPTION_LABEL : originalAction;
      this.prevChoice_ = dropdown.value;
    });
  }

  /**
   * Populate dropdown menu choices.
   */
  private setUpButtonMapTargets_(): void {
    this.buttonMapTargets_ = [];
    if (!this.actionList) {
      return;
    }
    // TODO(yyhyyh@): Get buttonMapTargets_ from provider in customization
    // pages, and pass it as a value instead of creating fake data here.
    for (const actionChoice of this.actionList) {
      const acceleratorAction = actionChoice.actionType.acceleratorAction;
      const staticShortcutAction = actionChoice.actionType.staticShortcutAction;
      if (acceleratorAction !== undefined) {
        // Prepend an acceleratorAction prefix to distinguish it from the
        // StaticShortcutAction enum.
        this.buttonMapTargets_.push({
          value: ACCELERATOR_ACTION_PREFIX + acceleratorAction.toString(),
          name: actionChoice.name,
        });
      } else if (staticShortcutAction !== undefined) {
        // Prepend a staticShortcutAction prefix to distinguish it from the
        // AcceleratorAction enum.
        this.buttonMapTargets_.push({
          value:
              STATICS_SHORTCUT_ACTION_PREFIX + staticShortcutAction.toString(),
          name: actionChoice.name,
        });
      }
    }
  }

  /**
   * Populate the button remapping action according to the existing settings.
   */
  private setUpRemappingActions_(): void {
    const dropdown = this.$.remappingActionDropdown;

    // Set the dropdown option label to default 'Key combination'.
    this.keyCombinationLabel_ = this.i18n('keyCombinationOptionLabel');

    // For accelerator actions, the remappingAction.acceleratorAction value is
    // number.
    const acceleratorAction =
        this.buttonRemapping_.remappingAction?.acceleratorAction;
    const keyEvent = this.buttonRemapping_.remappingAction?.keyEvent;
    // For static shortcut actions, the remappingAction.staticShortcutAction
    // value is number.
    const staticShortcutAction =
        this.buttonRemapping_.remappingAction?.staticShortcutAction;
    if (acceleratorAction !== undefined && !isNaN(acceleratorAction)) {
      // Prepend an acceleratorAction prefix to distinguish it from the
      // staticShortcutAction enum.
      const originalAcceleratorAction =
          ACCELERATOR_ACTION_PREFIX + acceleratorAction.toString();
      this.initializeDropdown_(originalAcceleratorAction, dropdown);
    } else if (keyEvent) {
      this.set('fakePref_.value', KEY_COMBINATION_OPTION_LABEL);
      this.keyCombinationLabel_ = getKeyCombinationLabel(keyEvent) ??
          this.i18n('keyCombinationOptionLabel');

      microTask.run(() => {
        dropdown.value = KEY_COMBINATION_OPTION_LABEL;
        this.prevChoice_ = dropdown.value;
      });
    } else if (
        staticShortcutAction !== undefined && !isNaN(staticShortcutAction)) {
      // Prepend a staticShortcutAction prefix to distinguish it from
      // the acceleratorAction enum.
      const originalStaticShortcutAction =
          STATICS_SHORTCUT_ACTION_PREFIX + staticShortcutAction.toString();
      this.initializeDropdown_(originalStaticShortcutAction, dropdown);
    } else {
      this.set('fakePref_.value', NO_REMAPPING_OPTION_LABEL);
      microTask.run(() => {
        dropdown.value = NO_REMAPPING_OPTION_LABEL;
        this.prevChoice_ = dropdown.value;
      });
    }
  }

  /**
   * Initialize the button remapping content and set up fake pref.
   */
  private initializeCustomizeKey(): void {
    if (!this.buttonRemappingList ||
        !this.buttonRemappingList[this.remappingIndex]) {
      return;
    }
    this.isInitialized_ = false;
    this.buttonRemapping_ = this.buttonRemappingList[this.remappingIndex];
    this.setUpButtonMapTargets_();
    this.setUpRemappingActions_();
    this.isInitialized_ = true;
  }


  /**
   * This method is called when fakePref_.value is changed to
   * NO_REMAPPING_OPTION_LABEL or enums of remappingAction.
   *
   * @returns Updated button remapping with selected remapping action or
   * no remapping action.
   */
  private getUpdatedRemapping(): ButtonRemapping {
    if (this.fakePref_.value === NO_REMAPPING_OPTION_LABEL) {
      const updatedRemapping: ButtonRemapping = {
        name: this.buttonRemapping_.name,
        button: this.buttonRemapping_.button,
      };
      return updatedRemapping;
    }
    // Otherwise the button is remapped to a remappingAction.
    let remappingAction: RemappingAction|undefined = undefined;
    if (this.fakePref_.value.startsWith(ACCELERATOR_ACTION_PREFIX)) {
      // Remove the acceleratorAction prefix to get the real enum value.
      this.fakePref_.value =
          this.fakePref_.value.slice(ACCELERATOR_ACTION_PREFIX.length);
      remappingAction = {
        acceleratorAction: Number(this.fakePref_.value),
      };
    }
    if (this.fakePref_.value.startsWith(STATICS_SHORTCUT_ACTION_PREFIX)) {
      // Remove the staticShortcutAction prefix to get the real enum value.
      this.fakePref_.value =
          this.fakePref_.value.slice(STATICS_SHORTCUT_ACTION_PREFIX.length);
      remappingAction = {
        staticShortcutAction: Number(this.fakePref_.value),
      };
    }
    const updatedRemapping: ButtonRemapping = {
      ...this.buttonRemapping_,
      remappingAction,
    };
    return updatedRemapping;
  }

  /**
   * Update device settings whenever the pref changes.
   */
  private onSettingsChanged(): void {
    if (!this.isInitialized_) {
      return;
    }

    this.set(
        `buttonRemappingList.${this.remappingIndex}`,
        this.getUpdatedRemapping());
    this.dispatchEvent(new CustomEvent('button-remapping-changed', {
      bubbles: true,
      composed: true,
    }));
  }

  private onSelectChange_(): void {
    const select = this.$.remappingActionDropdown;
    if (select.value === OPEN_DIALOG_OPTION_LABEL) {
      this.dispatchEvent(new CustomEvent('show-key-combination-dialog', {
        bubbles: true,
        composed: true,
        detail: {buttonIndex: this.remappingIndex},
      }));
      microTask.run(() => {
        select.value = this.prevChoice_;
      });
    } else if (select!.value !== this.fakePref_.value) {
      this.set('fakePref_.value', select!.value);
      this.prevChoice_ = select.value;
    }
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
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonRowElement.is]: CustomizeButtonRowElement;
  }
}

customElements.define(CustomizeButtonRowElement.is, CustomizeButtonRowElement);
