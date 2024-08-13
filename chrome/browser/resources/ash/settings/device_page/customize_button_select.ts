// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/shortcut_input_ui/icons.html.js';
import './input_device_settings_shared.css.js';
import './customize_button_dropdown_item.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {LWIN_KEY, META_KEY, ShortcutInputKeyElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import {KeyToIconNameMap} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
// <if expr="_google_chrome" >
import {KeyToInternalIconNameMap, KeyToInternalIconNameRefreshOnlyMap} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_utils.js';
// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';

import {CustomizeButtonDropdownItemElement, DropdownItemSelectEvent, DropdownMenuOption} from './customize_button_dropdown_item.js';
import {getTemplate} from './customize_button_select.html.js';
import {ActionChoice, ButtonRemapping, KeyEvent, MetaKey, RemappingAction, StaticShortcutAction} from './input_device_settings_types.js';

export interface CustomizeButtonSelectElement {
  $: {
    selectDropdown: HTMLButtonElement,
    menuContainer: HTMLDivElement,
  };
}


export const NO_REMAPPING_OPTION_VALUE = 'none';
export const KEY_COMBINATION_OPTION_VALUE = 'key combination';
export const OPEN_DIALOG_OPTION_VALUE = 'open key combination dialog';

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

/**
 * Converts a keyEvent to a string representing all the modifiers and the vkey.
 */
function getInputKeys(keyEvent: KeyEvent): string[] {
  const inputKeysArray: string[] = [];
  modifierBitMaskToString.forEach((modifierName: string, bitValue: number) => {
    if ((keyEvent.modifiers & bitValue) !== 0) {
      inputKeysArray.push(modifierName, '+');
    }
  });

  // Now if pressing a single modifier key like "shift", it will show
  // "shift + shift" instead of a single "shift".
  // Temporarily add a condition to check modifierName duplicating with
  // keyDisplay until it's fixed in the key combination logics.
  // TODO(yyhyyh@): Remove the condition when it's fixed in backend.
  if (keyEvent.keyDisplay !== undefined && keyEvent.keyDisplay.length !== 0 &&
      !inputKeysArray.includes(keyEvent.keyDisplay.toLowerCase())) {
    inputKeysArray.push(keyEvent.keyDisplay);
  } else {
    // If no regular key to display, remove the extra '+'.
    inputKeysArray.splice(inputKeysArray.length - 1, 1);
  }
  return inputKeysArray;
}

/**
 * @fileoverview
 * 'customize-button-select' contains all the remapping actions for a
 * button. The user can click the component to display the dropdown menu
 * and select an action to customize the remapped button.
 */

declare global {
  interface HTMLElementEventMap {
    'customize-button-dropdown-selected': DropdownItemSelectEvent;
  }
}

const CustomizeButtonSelectElementBase = I18nMixin(PolymerElement);

export class CustomizeButtonSelectElement extends
    CustomizeButtonSelectElementBase {
  static get is() {
    return 'customize-button-select' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      menu: {
        type: Object,
      },

      shouldShowDropdownMenu_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      buttonRemappingList: {
        type: Array,
      },

      remappingIndex: {
        type: Number,
      },

      buttonRemapping_: {
        type: Object,
      },

      actionList: {
        type: Array,
      },

      selectedValue: {
        type: String,
        value: NO_REMAPPING_OPTION_VALUE,
      },

      label_: {
        type: String,
        computed: 'getSelectedLabel_(selectedValue, menu.*)',
      },

      inputKeys_: {
        type: Array,
        value: [],
      },

      remappedToKeyCombination_: {
        type: Boolean,
        value: false,
      },

      highlightedValue_: {
        type: String,
        value: '',
      },

      focusTarget_: {
        type: Object,
        value: undefined,
      },

      metaKey: Object,
    };
  }

  static get observers(): string[] {
    return [
      'onSettingsChanged(selectedValue)',
      'initializeButtonSelect_(buttonRemappingList.*, remappingIndex, ' +
          'actionList)',
    ];
  }

  menu: DropdownMenuOptionList;
  buttonRemappingList: ButtonRemapping[];
  remappingIndex: number;
  actionList: ActionChoice[];
  selectedValue: string;
  metaKey: MetaKey = MetaKey.kSearch;
  private isInitialized_: boolean;
  private shouldShowDropdownMenu_: boolean;
  private label_: string;
  private buttonRemapping_: ButtonRemapping;
  private inputKeys_: string[];
  private remappedToKeyCombination_: boolean;
  private highlightedValue_: string;
  private focusTarget_: CustomizeButtonDropdownItemElement;

  override connectedCallback(): void {
    super.connectedCallback();
    this.addEventListener('blur', this.onBlur_);
    this.addEventListener(
        'customize-button-dropdown-selected', this.onDropdownItemSelected_);
    this.addEventListener('keydown', this.onKeyDown_);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.removeEventListener('blur', this.onBlur_);
    this.removeEventListener(
        'customize-button-dropdown-selected', this.onDropdownItemSelected_);
    this.removeEventListener('keydown', this.onKeyDown_);
  }

  override focus(): void {
    this.$.selectDropdown.focus();
  }

  private initializeButtonSelect_(): void {
    if (!this.buttonRemappingList ||
        !this.buttonRemappingList[this.remappingIndex] || !this.actionList) {
      return;
    }

    this.isInitialized_ = false;
    this.buttonRemapping_ = this.buttonRemappingList[this.remappingIndex];
    this.setUpMenuItems_();
    this.initializeSelectedValue_();
    this.isInitialized_ = true;
  }

  private showDropdownMenu_(): void {
    if (!this.menu) {
      this.shouldShowDropdownMenu_ = true;
      return;
    }

    // Focus the selected Row.
    assert(!!this.selectedValue, 'There should be a selected item already.');
    this.highlightedValue_ =
        this.selectedValue === KEY_COMBINATION_OPTION_VALUE ?
        OPEN_DIALOG_OPTION_VALUE :
        this.selectedValue;
    const indexOfCurrentRow = this.menu.findIndex(
        (action: DropdownMenuOption) =>
            action.value === this.highlightedValue_);
    const dropdownMenuOptions =
        this.$.menuContainer.querySelectorAll('customize-button-dropdown-item');
    assert(!!dropdownMenuOptions[indexOfCurrentRow]);

    this.set('focusTarget_', dropdownMenuOptions[indexOfCurrentRow]);
    this.shouldShowDropdownMenu_ = true;
  }

  private onBlur_(): void {
    this.highlightedValue_ = '';
    this.shouldShowDropdownMenu_ = false;
  }

  private hideDropdownMenu_(): void {
    this.highlightedValue_ = '';
    this.shouldShowDropdownMenu_ = false;
    this.focus();
  }

  private onDropdownItemSelected_(e: DropdownItemSelectEvent): void {
    const optionValue = e.detail.value ?? NO_REMAPPING_OPTION_VALUE;
    if (optionValue === OPEN_DIALOG_OPTION_VALUE) {
      this.dispatchEvent(new CustomEvent('show-key-combination-dialog', {
        bubbles: true,
        composed: true,
        detail: {buttonIndex: this.remappingIndex},
      }));
    } else if (optionValue !== this.selectedValue) {
      this.set('selectedValue', optionValue);
    }

    // Close dropdown menu after selected.
    this.hideDropdownMenu_();
  }

  private getSelectedLabel_(): string {
    if (!this.selectedValue || !this.menu) {
      return this.i18n('noRemappingOptionLabel');
    }

    return this.findOptionInMenu_(this.selectedValue)?.name ??
        this.i18n('noRemappingOptionLabel');
  }

  private findOptionInMenu_(targetValue: number|string): DropdownMenuOption
      |undefined {
    return this.menu.find(
        (option: DropdownMenuOption) => option.value === targetValue);
  }

  setSelectedValue(newValue: string): void {
    const foundOption = this.menu.find(
        (dropdownItem: DropdownMenuOption) => dropdownItem.value === newValue);

    const foundValue = foundOption === undefined ? NO_REMAPPING_OPTION_VALUE :
                                                   foundOption.value;

    this.set('selectedValue', foundValue);
  }

  private setUpMenuItems_(): void {
    if (!this.actionList) {
      return;
    }
    const tempMenu = [];

    // Put default action to the top of dropdown menu per UX requirement.
    tempMenu.push({
      value: NO_REMAPPING_OPTION_VALUE,
      name: this.i18n('noRemappingOptionLabel'),
    });

    // Fill the dropdown menu with actionList.
    for (const actionChoice of this.actionList) {
      const acceleratorAction = actionChoice.actionType.acceleratorAction;
      const staticShortcutAction = actionChoice.actionType.staticShortcutAction;
      if (acceleratorAction !== undefined) {
        // Prepend an acceleratorAction prefix to distinguish it from the
        // StaticShortcutAction enum.
        tempMenu.push({
          value: ACCELERATOR_ACTION_PREFIX + acceleratorAction.toString(),
          name: actionChoice.name,
        });
      } else if (staticShortcutAction !== undefined) {
        // Prepend a staticShortcutAction prefix to distinguish it from the
        // AcceleratorAction enum.
        tempMenu.push({
          value:
              STATICS_SHORTCUT_ACTION_PREFIX + staticShortcutAction.toString(),
          name: actionChoice.name,
        });
      }
    }

    // Put 'Key combination' option in the dropdown menu.
    tempMenu.push({
      value: OPEN_DIALOG_OPTION_VALUE,
      name: this.i18n('keyCombinationOptionLabel'),
    });

    // Put kDisable action to the end of dropdown menu per UX requirement.
    tempMenu.push({
      value: STATICS_SHORTCUT_ACTION_PREFIX + StaticShortcutAction.kDisable,
      name: this.i18n('disbableOptionLabel'),
    });

    // This option is hidden in the menu because it's only displayed on
    // customize-button-select.
    tempMenu.push({
      value: KEY_COMBINATION_OPTION_VALUE,
      name: this.i18n('keyCombinationOptionLabel'),
      hidden: true,
    });

    this.set('menu', tempMenu);
  }

  private initializeSelectedValue_(): void {
    this.inputKeys_ = [];
    this.remappedToKeyCombination_ = false;
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
      this.setSelectedValue(
          ACCELERATOR_ACTION_PREFIX + acceleratorAction.toString());
    } else if (keyEvent) {
      this.inputKeys_ = getInputKeys(keyEvent);
      this.remappedToKeyCombination_ = !!this.inputKeys_;

      this.setSelectedValue(KEY_COMBINATION_OPTION_VALUE);
    } else if (
        staticShortcutAction !== undefined && !isNaN(staticShortcutAction)) {
      // Prepend a staticShortcutAction prefix to distinguish it from
      // the acceleratorAction enum.
      const originalStaticShortcutAction =
          STATICS_SHORTCUT_ACTION_PREFIX + staticShortcutAction.toString();
      this.setSelectedValue(originalStaticShortcutAction);
    } else {
      this.setSelectedValue(NO_REMAPPING_OPTION_VALUE);
    }
  }

  /**
   * This method is called when selectedvalue is changed to
   * NO_REMAPPING_OPTION_VALUE or enums of remappingAction.
   *
   * @returns Updated button remapping with selected remapping action or
   * no remapping action.
   */
  private getUpdatedRemapping(): ButtonRemapping {
    if (this.selectedValue === NO_REMAPPING_OPTION_VALUE) {
      const updatedRemapping: ButtonRemapping = {
        name: this.buttonRemapping_.name,
        button: this.buttonRemapping_.button,
        remappingAction: null,
      };
      return updatedRemapping;
    }
    // Otherwise the button is remapped to a remappingAction.
    let remappingAction: RemappingAction|null = null;
    if (this.selectedValue.startsWith(ACCELERATOR_ACTION_PREFIX)) {
      // Remove the acceleratorAction prefix to get the real enum value.
      remappingAction = {
        acceleratorAction:
            Number(this.selectedValue.slice(ACCELERATOR_ACTION_PREFIX.length)),
      };
    }
    if (this.selectedValue.startsWith(STATICS_SHORTCUT_ACTION_PREFIX)) {
      // Remove the staticShortcutAction prefix to get the real enum value.
      remappingAction = {
        staticShortcutAction: Number(
            this.selectedValue.slice(STATICS_SHORTCUT_ACTION_PREFIX.length)),
      };
    }
    const updatedRemapping: ButtonRemapping = {
      ...this.buttonRemapping_,
      remappingAction,
    };
    return updatedRemapping;
  }

  /**
   * Update device settings whenever the selectedValue changes.
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

  private getIconIdForKey_(key: string): string|null {
    if (key === META_KEY || key === LWIN_KEY) {
      switch (this.metaKey) {
        case MetaKey.kSearch:
          return 'shortcut-input-keys:search';
        case MetaKey.kLauncherRefresh:
          return 'ash-internal:launcher-refresh';
        default:
          return 'shortcut-input-keys:launcher';
      }
    }

    // <if expr="_google_chrome" >
    const internalIconName = KeyToInternalIconNameMap[key];
    if (internalIconName) {
      return `ash-internal:${internalIconName}`;
    }

    const internalRefreshIconName = KeyToInternalIconNameRefreshOnlyMap[key];
    if (internalRefreshIconName && this.metaKey === MetaKey.kLauncherRefresh) {
      return `ash-internal:${internalRefreshIconName}`;
    }
    // </if>

    const iconName = KeyToIconNameMap[key];
    if (iconName) {
      return `shortcut-input-keys:${iconName}`;
    }

    return null;
  }

  private getAriaLabelForIcon(key: string): string {
    const ariaLabelStringId =
        ShortcutInputKeyElement.getAriaLabelStringId(key, this.metaKey);

    return this.i18n(ariaLabelStringId);
  }

  /**
   * Return true if the item in the dropdown menu is selected.
   */
  private isItemSelected_(item: DropdownMenuOption): boolean {
    if (item.value === OPEN_DIALOG_OPTION_VALUE &&
        this.selectedValue === KEY_COMBINATION_OPTION_VALUE) {
      return true;
    }
    return item.value === this.selectedValue;
  }

  private onKeyDown_(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      if (this.shouldShowDropdownMenu_) {
        this.updateDropdownSelection_();
      } else {
        this.showDropdownMenu_();
      }
      return;
    }

    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      e.preventDefault();
      this.selectRowViaKeys(e.key);
      return;
    }

    if (e.key === 'Escape') {
      this.hideDropdownMenu_();
    }
  }

  private selectRowViaKeys(key: string): void {
    assert(key === 'ArrowDown' || key === 'ArrowUp', 'Only arrow keys.');

    // Display the dropdown menu if it's not popped out.
    if (!this.shouldShowDropdownMenu_) {
      this.showDropdownMenu_();
      return;
    }

    assert(
        !!this.highlightedValue_,
        'There should be a highlighted item already.');
    // Select the new item.
    const indexOfCurrentRow = this.menu.findIndex(
        (action: DropdownMenuOption) =>
            action.value === this.highlightedValue_);
    // Skipping the hidden option for key combination label display.
    const numRows = this.menu.length - 1;
    const delta = key === 'ArrowUp' ? -1 : 1;
    const indexOfNewRow = (numRows + indexOfCurrentRow + delta) % numRows;

    const dropdownMenuOptions =
        this.$.menuContainer.querySelectorAll('customize-button-dropdown-item');

    assert(!!dropdownMenuOptions[indexOfNewRow]);
    dropdownMenuOptions[indexOfNewRow]?.focus();
    dropdownMenuOptions[indexOfNewRow]?.scrollIntoViewIfNeeded();

    // Update the highlighted value.
    this.highlightedValue_ = this.menu[indexOfNewRow]!.value as string;
  }

  private updateDropdownSelection_(): void {
    if (!!this.highlightedValue_ && this.highlightedValue_ !== '' &&
        this.menu?.findIndex(
            (action: DropdownMenuOption) =>
                action.value === this.highlightedValue_) >= 0) {
      this.dispatchEvent(new CustomEvent('customize-button-dropdown-selected', {
        bubbles: true,
        composed: true,
        detail: {
          value: this.highlightedValue_,
        },
      }));
    }

    this.hideDropdownMenu_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonSelectElement.is]: CustomizeButtonSelectElement;
  }
}

customElements.define(
    CustomizeButtonSelectElement.is, CustomizeButtonSelectElement);
