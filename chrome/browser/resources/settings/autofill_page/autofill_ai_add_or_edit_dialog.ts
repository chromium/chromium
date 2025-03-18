// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-ai-add-or-edit-dialog' is the dialog that
 * allows adding and editing entity instances for Autofill AI.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '../settings_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './autofill_ai_add_or_edit_dialog.html.js';
import type {CountryDetailManagerProxy} from './country_detail_manager_proxy.js';
import {CountryDetailManagerProxyImpl} from './country_detail_manager_proxy.js';
import type {EntityDataManagerProxy} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type AttributeInstance = chrome.autofillPrivate.AttributeInstance;
type AttributeType = chrome.autofillPrivate.AttributeType;
const AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;
type CountryEntry = chrome.autofillPrivate.CountryEntry;
type EntityInstance = chrome.autofillPrivate.EntityInstance;

export interface SettingsAutofillAiAddOrEditDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}


export class SettingsAutofillAiAddOrEditDialogElement extends PolymerElement {
  static get is() {
    return 'settings-autofill-ai-add-or-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
         The entity instance to be modified. If this is an "add" dialog, the
         entity instance has only a type, but no attribute instances or guid.
       */
      entityInstance: {
        type: Object,
        value: null,
      },

      dialogTitle: {
        type: String,
        value: '',
      },

      /**
         Complete list of attribute instances that are associated with the
         current entity instance. If this is an "edit" dialog, some attribute
         instances are populated with their already existing values.
       */
      completeAttributeInstanceList_: {
        type: Array,
        computed: 'computeCompleteAttributeInstanceList_(countryList_, ' +
            'completeAttributeTypesList_)',
      },

      /**
         The list of all countries that should be displayed in a <select>
         element for a country field.
       */
      countryList_: {
        type: Array,
        value: () => [],
      },

      /**
         Complete list of attribute types that are associated with the
         current entity type.
       */
      completeAttributeTypesList_: {
        type: Array,
        value: () => [],
      },

      /**
         False if the form is invalid. The first validation occurs when the user
         clicks the "Save" button for the first time. Subsequent validations
         occur any time an input field is changed. If false, the "Save" button
         is disabled and an error message is displayed.
       */
      canSave_: {
        type: Boolean,
        value: true,
      },
    };
  }

  entityInstance: EntityInstance|null;
  dialogTitle: string;
  private completeAttributeInstanceList_: AttributeInstance[];
  private countryList_: CountryEntry[];
  private completeAttributeTypesList_: AttributeType[];
  private canSave_: boolean;
  private userClickedSaveButton_: boolean = false;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();
  private countryDetailManager_: CountryDetailManagerProxy =
      CountryDetailManagerProxyImpl.getInstance();

  override async connectedCallback(): Promise<void> {
    super.connectedCallback();

    this.countryList_ = await this.countryDetailManager_.getCountryList(
        /*forAccountStorage=*/ false);

    assert(this.entityInstance);
    this.completeAttributeTypesList_ =
        await this.entityDataManager_.getAllAttributeTypesForEntityTypeName(
            this.entityInstance.type.typeName);
  }

  private computeCompleteAttributeInstanceList_(): AttributeInstance[] {
    if (this.countryList_.length === 0 ||
        this.completeAttributeTypesList_.length === 0) {
      return [];
    }

    return this.completeAttributeTypesList_.map(attributeType => {
      assert(this.entityInstance);
      const existingAttributeInstance =
          this.entityInstance.attributeInstances.find(
              existingAttributeInstance =>
                  existingAttributeInstance.type.typeName ===
                  attributeType.typeName);
      this.convertCountryAttributeInstance_(existingAttributeInstance);
      return {
        type: attributeType,
        value: existingAttributeInstance?.value || '',
      };
    });
  }

  private convertCountryAttributeInstance_(
      attributeInstace: AttributeInstance|undefined): void {
    if (!attributeInstace) {
      return;
    }
    // If `entityInstance` has a value stored for the country attribute, the
    // value will be the country name, not the country code. I.e. The value will
    // be "Germany", not "DE". On the other hand, the value stored into
    // `completeAttributeInstanceList_` should be the country code, not the
    // country name. I.e. The value should be "DE", not "Germany".
    // This logic exists because of a trade-off in the C++ autofill private API,
    // that has to call `EntityInstance::GetCompleteInfo()`, instead of
    // `EntityInstance::GetRawInfo()`.
    if (attributeInstace.type.dataType === AttributeTypeDataType.COUNTRY) {
      // TODO(crbug.com/403312087): Remove comment and exclamation marks once
      // the <hr> TODO below is solved.
      // The find operation will always find a match. Currently, the only entry
      // that doesn't have a name or a country code is the separator.
      attributeInstace.value = this.countryList_
                                   .find(
                                       country => attributeInstace.value ===
                                           country.name)!.countryCode!;
    }
  }

  private isCountryDataType_(attributeInstace: AttributeInstance): boolean {
    return attributeInstace.type.dataType === AttributeTypeDataType.COUNTRY;
  }

  private isStringDataType_(attributeInstace: AttributeInstance): boolean {
    // TODO(crbug.com/393318914): Handle dates separately.
    return attributeInstace.type.dataType === AttributeTypeDataType.STRING ||
        attributeInstace.type.dataType === AttributeTypeDataType.DATE;
  }

  private getCountryCode_(country: CountryEntry): string {
    // In case there is no country code, the string does not matter as long as
    // it is not empty and does not collide with any other country code.
    return country.countryCode || 'SEPARATOR';
  }

  private getCountryName_(country: CountryEntry): string {
    // TODO(crbug.com/403312087): Use <hr> as a separator, instead of hacking
    // the separator like this. To accommodate this, potentially refactor the
    // `CountryDetailManagerProxy` to return separately the current country and
    // the list of all countries. Do the same for regular Autofill.
    return country.name || '------';
  }

  private isCountrySeparator_(country: CountryEntry): boolean {
    return !country.countryCode;
  }

  private isCountrySelected_(
      attributeInstance: AttributeInstance, country: CountryEntry): boolean {
    return attributeInstance.value === this.getCountryCode_(country);
  }

  private onCountrySelectChange_(e: DomRepeatEvent<AttributeInstance>) {
    this.completeAttributeInstanceList_[e.model.index].value =
        (e.target as HTMLSelectElement).value;
    this.onAttributeInstanceFieldInput_(e);
  }

  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }

  private onConfirmClick_(): void {
    this.userClickedSaveButton_ = true;
    this.updateCanSave_();
    if (this.canSave_) {
      this.dispatchEvent(new CustomEvent('autofill-ai-add-or-edit-done', {
        bubbles: true,
        composed: true,
        detail: {
          ...this.entityInstance,
          // Don't take into consideration empty strings or strings made out
          // only of whitespaces.
          attributeInstances: this.completeAttributeInstanceList_.filter(
              attributeInstance => attributeInstance.value.trim().length > 0),
        },
      }));
      this.$.dialog.close();
    }
  }

  private onAttributeInstanceFieldInput_(_e: Event): void {
    if (this.userClickedSaveButton_) {
      this.updateCanSave_();
    }
  }

  private updateCanSave_(): void {
    // Don't take into consideration empty strings or strings made out only of
    // whitespaces.
    this.canSave_ = this.completeAttributeInstanceList_.some(
        attributeInstance => attributeInstance.value.trim().length > 0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-ai-add-or-edit-dialog':
        SettingsAutofillAiAddOrEditDialogElement;
  }
}

customElements.define(
    SettingsAutofillAiAddOrEditDialogElement.is,
    SettingsAutofillAiAddOrEditDialogElement);
