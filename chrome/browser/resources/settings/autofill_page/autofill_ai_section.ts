// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-ai-section' contains configuration options
 * for Autofill AI.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';
import '../simple_confirmation_dialog.js';
import './autofill_ai_add_or_edit_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiEnterpriseFeaturePrefName, ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import {getTemplate} from './autofill_ai_section.html.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;
type EntityType = chrome.autofillPrivate.EntityType;

export interface SettingsAutofillAiSectionElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    addMenu: CrLazyRenderElement<CrActionMenuElement>,
    prefToggle: SettingsToggleButtonElement,
  };
}

const SettingsAutofillAiSectionElementBase =
    I18nMixin(PrefsMixin(PolymerElement));

export class SettingsAutofillAiSectionElement extends
    SettingsAutofillAiSectionElementBase {
  static get is() {
    return 'settings-autofill-ai-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
         If a user is not eligible for Autofill with Ai, but they have data
         saved, the code allows them only to edit and delete their data. They
         are not allowed to add new data, or to opt-in or opt-out of Autofill
         with Ai using the toggle at the top of this page.
         If a user is not eligible for Autofill with Ai and they also have no
         data saved, then they cannot access this page at all.
       */
      ineligibleUser: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
         A "fake" preference object that reflects the state of the opt-in
         toggle and the presence/absence of an enterprise policy.
         This allows leveraging the settings-toggle-button component
         to reflect enterprise enabled/disabled states.
       */
      optedIn_: {
        type: Object,
        value: () => ({
          // Does not correspond to an actual pref - this is faked to allow
          // writing it into a GAIA-id keyed dictionary of opt-ins.
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        }),
      },

      /**
         The corresponding `EntityInstance` model for any entity instance
         related action menus or dialogs.
       */
      activeEntityInstance_: {
        type: Object,
        value: null,
      },

      /**
         Complete list of entity types that exist. When the user wants to add a
         new entity instance, this list is displayed.
       */
      completeEntityTypesList_: {
        type: Array,
        value: () => [],
      },

      /**
         The same dialog can be used for both adding and editing entity
         instances.
       */
      showAddOrEditEntityInstanceDialog_: {
        type: Boolean,
        value: false,
      },

      addOrEditEntityInstanceDialogTitle_: {
        type: String,
        value: '',
      },

      showRemoveEntityInstanceDialog_: {
        type: Boolean,
        value: false,
      },

      entityInstances_: {
        type: Array,
        value: () => [],
      },
    };
  }

  declare ineligibleUser: boolean;
  declare private optedIn_: chrome.settingsPrivate.PrefObject;
  declare private activeEntityInstance_: EntityInstance|null;
  declare private completeEntityTypesList_: EntityType[];
  declare private showAddOrEditEntityInstanceDialog_: boolean;
  declare private addOrEditEntityInstanceDialogTitle_: string;
  declare private showRemoveEntityInstanceDialog_: boolean;
  declare private entityInstances_: EntityInstanceWithLabels[];

  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.entityDataManager_.getOptInStatus().then(
        optedIn => this.set('optedIn_.value', !this.ineligibleUser && optedIn));
    const policyDisabled =
        this.getPref(AiEnterpriseFeaturePrefName.AUTOFILL_AI).value ===
        ModelExecutionEnterprisePolicyValue.DISABLE;
    if (policyDisabled) {
      this.set(
          'optedIn_.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
      this.set(
          'optedIn_.controlledBy',
          chrome.settingsPrivate.ControlledBy.USER_POLICY);
    }

    this.entityInstancesChangedListener_ =
        (entityInstances => this.entityInstances_ =
             entityInstances.sort(this.entityInstancesWithLabelsComparator_));
    this.entityDataManager_.addEntityInstancesChangedListener(
        this.entityInstancesChangedListener_);

    this.entityDataManager_.getAllEntityTypes().then(
        (entityTypes: EntityType[]) => {
          this.completeEntityTypesList_ =
              entityTypes.sort(this.entityTypesComparator_);
        });

    this.entityDataManager_.loadEntityInstances().then(
        (entityInstances: EntityInstanceWithLabels[]) => this.entityInstances_ =
            entityInstances.sort(this.entityInstancesWithLabelsComparator_));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.entityInstancesChangedListener_);
    this.entityDataManager_.removeEntityInstancesChangedListener(
        this.entityInstancesChangedListener_);
    this.entityInstancesChangedListener_ = null;
  }

  /*
   * This comparator purposefully uses sensitivity 'base', not to differentiate
   * between different capitalization or diacritics.
   */
  private entityTypesComparator_(a: EntityType, b: EntityType) {
    return a.typeNameAsString.localeCompare(
        b.typeNameAsString, undefined, {sensitivity: 'base'});
  }

  /**
   * This comparator compares the labels alphabetically, and, in case of
   * equality, the sublabels.
   * This comparator purposefully uses sensitivity 'base', not to differentiate
   * between different capitalization or diacritics.
   */
  private entityInstancesWithLabelsComparator_(
      a: EntityInstanceWithLabels, b: EntityInstanceWithLabels) {
    return (a.entityInstanceLabel + a.entityInstanceSubLabel)
        .localeCompare(
            b.entityInstanceLabel + b.entityInstanceSubLabel, undefined,
            {sensitivity: 'base'});
  }

  private async onOptInToggleChange_() {
    // `setOptInStatus` returns false when the user tries to toggle the opt-in
    // status when they're ineligible.  This shouldn't happen usually but in
    // some cases it can happen (see crbug.com/408145195).
    this.ineligibleUser = !(await this.entityDataManager_.setOptInStatus(
        this.$.prefToggle.checked));
    if (this.ineligibleUser) {
      this.set('optedIn_.value', false);
    }
  }

  /**
   * Whether an info bullet regarding logging is shown. Autofill Ai only shows
   * logging behaviour information for enterprise clients who have either the
   * feature disabled or just logging disabled.
   */
  private showLoggingInfoBullet_(pref: number) {
    return pref !== ModelExecutionEnterprisePolicyValue.ALLOW;
  }

  /**
   * Handles tapping on the "Add" entity instance button.
   */
  private onAddButtonClick_(e: Event) {
    const addButton = e.target as HTMLElement;
    this.$.addMenu.get().showAt(addButton, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  private onAddEntityInstanceFromDropdownClick_(e: DomRepeatEvent<EntityType>) {
    e.preventDefault();
    // Create a new entity instance with no attribute instances and guid. A guid
    // will be assigned after saving, on the C++ side.
    this.activeEntityInstance_ = {
      type: e.model.item,
      attributeInstances: [],
      guid: '',
      nickname: '',
    };
    this.addOrEditEntityInstanceDialogTitle_ =
        this.activeEntityInstance_.type.addEntityTypeString;
    this.showAddOrEditEntityInstanceDialog_ = true;
    this.$.addMenu.get().close();
  }

  /**
   * Open the action menu.
   */
  private async onMoreButtonClick_(
      e: DomRepeatEvent<EntityInstanceWithLabels>) {
    const moreButton = e.target as HTMLElement;
    this.activeEntityInstance_ =
        await this.entityDataManager_.getEntityInstanceByGuid(
            e.model.item.guid);
    this.$.actionMenu.get().showAt(moreButton);
  }

  /**
   * Handles tapping on the "Edit" entity instance button in the action menu.
   */
  private onMenuEditEntityInstanceClick_(e: Event) {
    e.preventDefault();
    assert(this.activeEntityInstance_);
    this.addOrEditEntityInstanceDialogTitle_ =
        this.activeEntityInstance_.type.editEntityTypeString;
    this.showAddOrEditEntityInstanceDialog_ = true;
    this.$.actionMenu.get().close();
  }

  /**
   * Handles tapping on the "Delete" entity instance button in the action menu.
   */
  private onMenuRemoveEntityInstanceClick_(e: Event) {
    e.preventDefault();
    this.showRemoveEntityInstanceDialog_ = true;
    this.$.actionMenu.get().close();
  }

  private onAutofillAiAddOrEditDone_(e: CustomEvent<EntityInstance>) {
    e.stopPropagation();
    this.entityDataManager_.addOrUpdateEntityInstance(e.detail);
  }

  private onAddOrEditEntityInstanceDialogClose_(e: Event) {
    e.stopPropagation();
    this.showAddOrEditEntityInstanceDialog_ = false;
    this.activeEntityInstance_ = null;
  }

  private onRemoveEntityInstanceDialogClose_() {
    const wasDeletionConfirmed =
        this.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#removeEntityInstanceDialog')!.wasConfirmed();
    if (wasDeletionConfirmed) {
      assert(this.activeEntityInstance_);
      this.entityDataManager_.removeEntityInstance(
          this.activeEntityInstance_.guid);
    }
    this.showRemoveEntityInstanceDialog_ = false;
    this.activeEntityInstance_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-ai-section': SettingsAutofillAiSectionElement;
  }
}

customElements.define(
    SettingsAutofillAiSectionElement.is, SettingsAutofillAiSectionElement);
