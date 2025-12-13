// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-entries-list-element' contains configuration
 * options for Autofill AI.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../icons.html.js';
import '../settings_shared.css.js';
import '../simple_confirmation_dialog.js';
import './autofill_ai_add_or_edit_dialog.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';

import type {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import {getTemplate} from './autofill_ai_entries_list.html.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;
type EntityType = chrome.autofillPrivate.EntityType;

export interface SettingsAutofillAiEntriesListElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    addMenu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

const SettingsAutofillAiEntriesListElementBase = SettingsViewMixin(
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement))));

export class SettingsAutofillAiEntriesListElement extends
    SettingsAutofillAiEntriesListElementBase {
  static get is() {
    return 'settings-autofill-ai-entries-list';
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
        value() {
          return !loadTimeData.getBoolean('userEligibleForAutofillAi');
        },
      },

      allowedEntityTypes: {
        type: Set,
        value: null,
      },

      listTitle: {
        type: String,
      },

      /**
         Optional boolean preference used to determine the list's editability.
         If true - user will be able to add new entries to the list. Note that
         even if preference is true allows the user may still be prevented from
         adding entries due to other eligibility checks.
      */
      allowEditingPref: {
        type: Object,
        value: null,
      },

      allowEditing_: {
        type: Object,
        value: false,
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
      /**
        If true, Autofill AI does not depend on whether Autofill for addresses
        is enabled.
      */
      autofillAiIgnoresWhetherAddressFillingIsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'AutofillAiIgnoresWhetherAddressFillingIsEnabled');
        },
      },
    };
  }

  static get observers() {
    return [
      'onAutofillAddressPrefChanged_(' +
          'prefs.autofill.profile_enabled.value, allowEditingPref.*))',
      'onOptInStatusChanged_(' +
          'prefs.autofill.autofill_ai.opt_in_status.value, allowEditingPref.*)',
    ];
  }

  declare ineligibleUser: boolean;
  declare allowedEntityTypes: Set<EntityTypeName>|null;
  declare listTitle: string;
  declare allowEditingPref: chrome.settingsPrivate.PrefObject<boolean>|null;
  declare private allowEditing_: boolean;
  declare private activeEntityInstance_: EntityInstance|null;
  declare private completeEntityTypesList_: EntityType[];
  declare private showAddOrEditEntityInstanceDialog_: boolean;
  declare private addOrEditEntityInstanceDialogTitle_: string;
  declare private showRemoveEntityInstanceDialog_: boolean;
  declare private entityInstances_: EntityInstanceWithLabels[];
  declare private autofillAiIgnoresWhetherAddressFillingIsEnabled_: boolean;

  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.entityDataManager_.getOptInStatus().then(
        optedInAtofillAi => this.allowEditing_ = !this.ineligibleUser &&
            optedInAtofillAi && this.isEditingAllowedByPref_);

    this.entityInstancesChangedListener_ =
        (entityInstances: EntityInstanceWithLabels[]) => {
          // Filter only if the filter was set
          const filteredEntityInstaces = this.allowedEntityTypes ?
              entityInstances.filter(
                  instance =>
                      this.allowedEntityTypes!.has(instance.type.typeName)) :
              entityInstances;

          this.entityInstances_ = filteredEntityInstaces.sort(
              this.entityInstancesWithLabelsComparator_);
        };

    this.entityDataManager_.loadEntityInstances().then(
        this.entityInstancesChangedListener_);

    this.entityDataManager_.addEntityInstancesChangedListener(
        this.entityInstancesChangedListener_);

    this.entityDataManager_.getWritableEntityTypes().then(
        (entityTypes: EntityType[]) => {
          this.updateEntittyTypesList_(entityTypes);
        });

    this.addWebUiListener(
        'sync-status-changed', this.onSyncStatusChanged_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.entityInstancesChangedListener_);
    this.entityDataManager_.removeEntityInstancesChangedListener(
        this.entityInstancesChangedListener_);
    this.entityInstancesChangedListener_ = null;
  }

  private updateEntittyTypesList_(entityTypes: EntityType[]) {
    // Filter only if the filter was set
    const filteredEntities = this.allowedEntityTypes ?
        entityTypes.filter(
            instance => this.allowedEntityTypes!.has(instance.typeName)) :
        entityTypes;

    this.completeEntityTypesList_ =
        filteredEntities.sort(this.entityTypesComparator_);
  }

  /*
   * This comparator purposefully uses sensitivity 'base', not to differentiate
   * between different capitalization or diacritics.
   */
  private entityTypesComparator_(a: EntityType, b: EntityType): number {
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
      a: EntityInstanceWithLabels, b: EntityInstanceWithLabels): number {
    return (a.entityInstanceLabel + a.entityInstanceSubLabel)
        .localeCompare(
            b.entityInstanceLabel + b.entityInstanceSubLabel, undefined,
            {sensitivity: 'base'});
  }

  /**
   * Handles tapping on the "Add" entity instance button.
   */
  private onAddEntityInstanceClick_(e: Event) {
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

  // Adjusts the opt-in state when address autofill status changes.
  //
  // This covers the case where a user disables address autofill and then checks
  // the AutofillAI opt-in status. In this case, we do not remove the AutofillAI
  // entry, but just set the opt-in to false. Note that other
  // preconditions (e.g., sync) are not covered.
  private async onAutofillAddressPrefChanged_(prefValue: boolean) {
    if (this.autofillAiIgnoresWhetherAddressFillingIsEnabled_) {
      return;
    }
    const autofillAiOptInStatus =
        await this.entityDataManager_.getOptInStatus();
    this.allowEditing_ = !this.ineligibleUser && autofillAiOptInStatus &&
        prefValue && this.isEditingAllowedByPref_;
  }

  private onRemoteWalletPassesLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('walletPassesPageUrl'));
  }

  private async onOptInStatusChanged_(): Promise<void> {
    const optedIn = await this.entityDataManager_.getOptInStatus();
    this.allowEditing_ =
        !this.ineligibleUser && optedIn && this.isEditingAllowedByPref_;
  }

  // Refreshes the entity types list when the sync status changes.
  //
  // Updates the list to reflect whether the user is signed in (allowing the
  // creation of entity instances for types stored on the server) or signed
  // out (disallowing it).
  private onSyncStatusChanged_(_: SyncStatus) {
    this.entityDataManager_.getWritableEntityTypes().then(
        (entityTypes: EntityType[]) => {
          this.updateEntittyTypesList_(entityTypes);
        });
  }

  private get isEditingAllowedByPref_(): boolean {
    // Defaults to true if the pref is not provided, allowing addition of new
    // entries.
    return this.allowEditingPref?.value ?? true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-ai-entries-list': SettingsAutofillAiEntriesListElement;
  }
}

customElements.define(
    SettingsAutofillAiEntriesListElement.is,
    SettingsAutofillAiEntriesListElement);
