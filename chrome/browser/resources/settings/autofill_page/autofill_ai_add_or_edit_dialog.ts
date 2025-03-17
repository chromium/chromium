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
import '../settings_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './autofill_ai_add_or_edit_dialog.html.js';
import type {EntityDataManagerProxy} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type AttributeInstance = chrome.autofillPrivate.AttributeInstance;
type AttributeType = chrome.autofillPrivate.AttributeType;
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
  private canSave_: boolean;
  private userClickedSaveButton_: boolean = false;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();

    assert(this.entityInstance);
    this.entityDataManager_
        .getAllAttributeTypesForEntityTypeName(
            this.entityInstance.type.typeName)
        .then((attributeTypes: AttributeType[]) => {
          this.completeAttributeInstanceList_ =
              attributeTypes.map(attributeType => {
                assert(this.entityInstance);
                const existingAttributeInstance =
                    this.entityInstance.attributeInstances.find(
                        existingAttributeInstance =>
                            existingAttributeInstance.type.typeName ===
                            attributeType.typeName);
                return {
                  type: attributeType,
                  value: existingAttributeInstance?.value || '',
                };
              });
        });
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
