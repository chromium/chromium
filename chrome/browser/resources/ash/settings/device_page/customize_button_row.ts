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
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonPressObserverReceiver} from '../mojom-webui/input_device_settings_provider.mojom-webui.js';

import {getTemplate} from './customize_button_row.html.js';
import {FakeInputDeviceSettingsProvider} from './fake_input_device_settings_provider.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {ActionChoice, Button, ButtonRemapping, InputDeviceSettingsProviderInterface, KeyEvent} from './input_device_settings_types.js';
import {buttonsAreEqual} from './input_device_settings_utils.js';

const NO_REMAPPING_OPTION_LABEL = 'none';
const KEY_COMBINATION_OPTION_LABEL = 'key combination';

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

      /**
       * Name of the remapping.
       */
      buttonRemappingName_: {
        type: String,
        value: '',
        computed:
            'getButtonRemappingName_(buttonRemappingList.*, remappingIndex)',
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
  private keyCombinationLabel_: string;
  private buttonRemappingName_: string;
  private isInitialized_: boolean;
  private inputDeviceSettingsProvider_: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();

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
      this.buttonMapTargets_.push({
        value: actionChoice.actionId.toString(),
        name: actionChoice.name,
      });
    }
  }

  /**
   * Populate the button remapping action according to the existing settings.
   */
  private setUpRemappingActions_(): void {
    const dropdown = this.$.remappingActionDropdown;

    // Set the dropdown option label to default 'Key combination'.
    this.keyCombinationLabel_ = this.i18n('keyCombinationOptionLabel');

    // For accelerator actions, the remappingAction.action value is number.
    const action = this.buttonRemapping_.remappingAction?.action;
    const keyEvent = this.buttonRemapping_.remappingAction?.keyEvent;
    if (action !== undefined && !isNaN(action)) {
      const originalAction =
          this.buttonRemapping_.remappingAction!.action!.toString();


      // Initialize fakePref with the tablet settings mapping.
      this.set('fakePref_.value', originalAction);

      // Initialize dropdown menu selection to match the tablet settings.
      const option = this.buttonMapTargets_.find((dropdownItem) => {
        return dropdownItem.value === originalAction;
      });

      microTask.run(() => {
        dropdown.value =
            option === undefined ? NO_REMAPPING_OPTION_LABEL : originalAction;
      });
    } else if (keyEvent) {
      this.set('fakePref_.value', KEY_COMBINATION_OPTION_LABEL);
      this.keyCombinationLabel_ = getKeyCombinationLabel(keyEvent) ??
          this.i18n('keyCombinationOptionLabel');

      microTask.run(() => {
        dropdown.value = KEY_COMBINATION_OPTION_LABEL;
      });
    } else {
      this.set('fakePref_.value', NO_REMAPPING_OPTION_LABEL);
      microTask.run(() => {
        dropdown.value = NO_REMAPPING_OPTION_LABEL;
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
    const updatedRemapping: ButtonRemapping = {
      ...this.buttonRemapping_,
      remappingAction: {
        action: Number(this.fakePref_.value),
      },
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
    if (select.value === KEY_COMBINATION_OPTION_LABEL) {
      // TODO(yyhyyh@): This event should be fired whenever users select the
      // 'key combination' option, even if no 'change' happens.
      this.dispatchEvent(new CustomEvent('show-key-combination-dialog', {
        bubbles: true,
        composed: true,
        detail: {buttonIndex: this.remappingIndex},
      }));
    } else if (select!.value !== this.fakePref_.value) {
      this.set('fakePref_.value', select!.value);
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
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonRowElement.is]: CustomizeButtonRowElement;
  }
}

customElements.define(CustomizeButtonRowElement.is, CustomizeButtonRowElement);
