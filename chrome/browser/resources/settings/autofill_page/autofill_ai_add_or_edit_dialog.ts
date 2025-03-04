// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-ai-add-or-edit-dialog' is the dialog that
 * allows adding and editing entities for Autofill AI.
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
         The entity to be modified. If this is an "add" dialog, the entity has
         only a type, but no attributes or guid.
       */
      entity: {
        type: Object,
        value: null,
      },

      dialogTitle: {
        type: String,
        value: '',
      },

      /**
         Complete list of attributes that are associated with the current
         entity. If this is an "edit" dialog, some attributes are populated with
         their already existing values.
       */
      completeAttributeList_: {
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

  entity: EntityInstance|null;
  dialogTitle: string;
  private completeAttributeList_: AttributeInstance[];
  private canSave_: boolean;
  private userClickedSaveButton_: boolean = false;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();

    assert(this.entity);
    this.entityDataManager_
        .getAllAttributeTypesForEntity(this.entity.type.typeName)
        .then((attributeTypes: AttributeType[]) => {
          this.completeAttributeList_ = attributeTypes.map(attributeType => {
            assert(this.entity);
            const existingAttribute = this.entity.attributes.find(
                existingAttribute =>
                    existingAttribute.type.typeName === attributeType.typeName);
            return {
              type: attributeType,
              value: existingAttribute?.value || '',
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
          ...this.entity,
          // Don't take into consideration empty strings or strings made out
          // only of whitespaces.
          attributes: this.completeAttributeList_.filter(
              attribute => attribute.value.trim().length > 0),
        },
      }));
      this.$.dialog.close();
    }
  }

  private onAttributeFieldInput_(_e: Event): void {
    if (this.userClickedSaveButton_) {
      this.updateCanSave_();
    }
  }

  private updateCanSave_(): void {
    // Don't take into consideration empty strings or strings made out only of
    // whitespaces.
    this.canSave_ = this.completeAttributeList_.some(
        attribute => attribute.value.trim().length > 0);
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
