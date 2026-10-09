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
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../simple_confirmation_dialog.js';
import './autofill_ai_add_or_edit_dialog.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import type {SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AiEnterpriseFeaturePrefName, ModelExecutionEnterprisePolicyValue} from '../ai_page/constants.js';
import {EntityTypeName} from '../autofill_ai_enums.mojom-webui.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import {getCss} from './autofill_ai_entries_list.css.js';
import {getHtml} from './autofill_ai_entries_list.html.js';
import type {EntityDataManagerProxy, EntityInstancesChangedListener} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;
type EntityType = chrome.autofillPrivate.EntityType;

export interface SettingsAutofillAiEntriesListElement {
  $: {
    actionMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    addMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

const SettingsAutofillAiEntriesListElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(
        I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement))));

export class SettingsAutofillAiEntriesListElement extends
    SettingsAutofillAiEntriesListElementBase {
  static get is() {
    return 'settings-autofill-ai-entries-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       Controls whether the user can use Autofill AI. For example this can be
       false if the extensions API disables the feature.
       Specifically in this file, it controls whether users can add new
       entities.
      */
      canEnableOrDisableAutofillAi_: {type: Boolean},

      allowedEntityTypes: {type: Object},

      listTitle: {type: String},

      pageName: {type: String},

      metricEntityTypes: {type: Object},

      /**
         Optional boolean preference used to determine the list's editability.
         If true - user will be able to add new entries to the list.

         Notes:
          * Even if preference is true the user may still be prevented from
            adding entries due to other eligibility checks.
          * We assume that the provided preference is controlled by the address
            autofill policy and extension API. If allowNewEntitiesAdditionPref
            is provided its value will be overridden by the address autofill
            preference when it is enforced.
      */
      allowNewEntitiesAdditionPref: {type: Object},

      allowNewEntitiesAddition_: {type: Boolean},

      /**
         The corresponding `EntityInstance` model for any entity instance
         related action menus or dialogs.
       */
      activeEntityInstance_: {type: Object},

      /**
         Complete list of entity types that exist. When the user wants to add a
         new entity instance, this list is displayed.
       */
      completeEntityTypesList_: {type: Array},

      /**
         The same dialog can be used for both adding and editing entity
         instances.
       */
      showAddOrEditEntityInstanceDialog_: {type: Boolean},

      addOrEditEntityInstanceDialogTitle_: {type: String},

      showRemoveEntityInstanceDialog_: {type: Boolean},

      activeEntityInstanceDeleteTitle_: {type: String},

      entityInstances_: {type: Array},

      /**
        If true, Autofill AI does not depend on whether Autofill for addresses
        is enabled.
      */
      autofillSettingsEnterprisePolicyEnabled_: {type: Boolean},
    };
  }

  accessor allowedEntityTypes: Set<EntityTypeName>|null = null;
  accessor listTitle: string = '';
  accessor pageName: string = '';
  accessor metricEntityTypes: Partial<Record<EntityTypeName, string>>|null =
      null;
  accessor allowNewEntitiesAdditionPref:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor allowNewEntitiesAddition_: boolean = false;
  protected accessor completeEntityTypesList_: EntityType[] = [];
  protected accessor activeEntityInstance_: EntityInstance|null = null;
  protected accessor showAddOrEditEntityInstanceDialog_: boolean = false;
  protected accessor addOrEditEntityInstanceDialogTitle_: string = '';
  protected accessor showRemoveEntityInstanceDialog_: boolean = false;
  protected accessor activeEntityInstanceDeleteTitle_: string = '';
  protected accessor entityInstances_: EntityInstanceWithLabels[] = [];
  private accessor autofillSettingsEnterprisePolicyEnabled_: boolean =
      loadTimeData.getBoolean('AutofillSettingsEnterprisePolicyEnabled');
  private accessor canEnableOrDisableAutofillAi_: boolean =
      loadTimeData.getBoolean('canEnableOrDisableAutofillAi');
  private activeEntityInstanceGuid_: string|null = null;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private entityInstancesChangedListener_: EntityInstancesChangedListener|null =
      null;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
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
          this.updateEntityTypesList_(entityTypes);
        });

    this.addWebUiListener(
        'sync-status-changed', this.onSyncStatusChanged_.bind(this));

    this.addPrefObserver(
        'autofill.profile_enabled',
        () => this.updateAllowNewEntitiesAddition_());
    this.addPrefObserver(
        AiEnterpriseFeaturePrefName.AUTOFILL_AI,
        () => this.updateAllowNewEntitiesAddition_());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.entityInstancesChangedListener_);
    this.entityDataManager_.removeEntityInstancesChangedListener(
        this.entityInstancesChangedListener_);
    this.entityInstancesChangedListener_ = null;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('allowNewEntitiesAdditionPref')) {
      this.updateAllowNewEntitiesAddition_();
    }
  }

  private updateEntityTypesList_(entityTypes: EntityType[]) {
    // Filter only if the filter was set
    const filteredEntities = this.allowedEntityTypes ?
        entityTypes.filter(
            instance => this.allowedEntityTypes!.has(instance.typeName)) :
        entityTypes;

    this.completeEntityTypesList_ =
        filteredEntities.sort(this.entityTypesComparator_);

    for (const entityType of this.completeEntityTypesList_) {
      if (entityType.supportsWalletStorage &&
          entityType.passType ===
              chrome.autofillPrivate.EntityPassType.PUBLIC_PASS) {
        this.entityDataManager_.preloadDetailsForUpsertPass(
            entityType.typeName);
      }
    }
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

  private getMetricEntityTypeString_(type: EntityTypeName): string {
    assert(this.metricEntityTypes);
    const metricString = this.metricEntityTypes[type];
    assert(metricString);
    return metricString;
  }

  /**
   * Handles tapping on the "Add" entity instance button.
   */
  protected onAddEntityInstanceClick_(e: Event) {
    const addButton = e.currentTarget as HTMLElement;
    this.$.addMenu.get().showAt(addButton, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  protected onAddEntityInstanceFromDropdownClick_(e: Event) {
    e.preventDefault();
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const item = this.completeEntityTypesList_[index];
    assert(item);
    if (this.pageName) {
      this.metricsBrowserProxy_.recordAction(`Settings.YourSavedInfo.${
          this.pageName}.Add.${
          this.getMetricEntityTypeString_(item.typeName as EntityTypeName)}`);
    }
    // Create a new entity instance with no attribute instances and guid. A guid
    // will be assigned after saving, on the C++ side.
    this.activeEntityInstance_ = {
      type: item,
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
  protected onMoreButtonClick_(e: Event) {
    const moreButton = e.currentTarget as HTMLElement;
    const index = Number(moreButton.dataset['index']);
    const item = this.entityInstances_[index];
    assert(item);
    this.activeEntityInstanceGuid_ = item.guid;
    this.$.actionMenu.get().showAt(moreButton);
  }

  /**
   * Handles tapping on the "Edit" entity instance button in the action menu.
   */
  protected async onMenuEditEntityInstanceClick_(e: Event) {
    e.preventDefault();

    const instanceWithLabels = this.entityInstances_.find(
        instance => instance.guid === this.activeEntityInstanceGuid_);

    if (this.pageName && instanceWithLabels) {
      this.metricsBrowserProxy_.recordAction(
          `Settings.YourSavedInfo.${this.pageName}.Edit.${
              this.getMetricEntityTypeString_(
                  instanceWithLabels.type.typeName as EntityTypeName)}`);
    }

    this.activeEntityInstance_ =
        await this.entityDataManager_.getEntityInstanceByGuid(
            this.activeEntityInstanceGuid_!);

    if (!this.activeEntityInstance_) {
      return;
    }

    this.addOrEditEntityInstanceDialogTitle_ =
        this.activeEntityInstance_.type.editEntityTypeString;
    this.showAddOrEditEntityInstanceDialog_ = true;
    this.$.actionMenu.get().close();
  }

  /**
   * Handles tapping on the "Delete" entity instance button in the action menu.
   */
  protected onMenuRemoveEntityInstanceClick_(e: Event) {
    e.preventDefault();

    const instanceWithLabels = this.entityInstances_.find(
        instance => instance.guid === this.activeEntityInstanceGuid_);
    if (!instanceWithLabels) {
      return;
    }

    if (this.pageName) {
      this.metricsBrowserProxy_.recordAction(
          `Settings.YourSavedInfo.${this.pageName}.Delete.${
              this.getMetricEntityTypeString_(
                  instanceWithLabels.type.typeName as EntityTypeName)}`);
    }

    this.activeEntityInstanceDeleteTitle_ =
        instanceWithLabels.type.deleteEntityTypeString;

    this.showRemoveEntityInstanceDialog_ = true;
    this.$.actionMenu.get().close();
  }

  protected onAutofillAiAddOrEditDone_(e: CustomEvent<EntityInstance>) {
    e.stopPropagation();
    // TODO(crbug.com/477845712): Remove this method once
    // `kAutofillAiWalletPrivatePasses` gets launched.
    if (!loadTimeData.getBoolean('enableAutofillAiWalletPrivatePasses')) {
      this.entityDataManager_.addOrUpdateEntityInstance(e.detail);
    }
  }

  protected onAddOrEditEntityInstanceDialogClose_(e: Event) {
    e.stopPropagation();
    this.showAddOrEditEntityInstanceDialog_ = false;
    this.activeEntityInstance_ = null;
  }

  protected onRemoveEntityInstanceDialogClose_() {
    const wasDeletionConfirmed =
        this.shadowRoot
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#removeEntityInstanceDialog')!.wasConfirmed();
    if (wasDeletionConfirmed) {
      this.entityDataManager_.removeEntityInstance(
          this.activeEntityInstanceGuid_!);
    }
    this.showRemoveEntityInstanceDialog_ = false;
    this.activeEntityInstanceGuid_ = null;
  }

  protected onRemoteWalletPassesLinkClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const item = this.entityInstances_[index];
    assert(item);
    assert(item.storedInWallet);
    assert(item.walletEntityUrl);
    OpenWindowProxyImpl.getInstance().openUrl(item.walletEntityUrl);
  }

  private async updateAllowNewEntitiesAddition_(): Promise<void> {
    const prefService = PrefService.getInstance();
    await prefService.whenInitialized();
    const addressPref =
        prefService.getPref<boolean>('autofill.profile_enabled');
    const autofillAiPref =
        prefService.getPref<ModelExecutionEnterprisePolicyValue>(
            AiEnterpriseFeaturePrefName.AUTOFILL_AI);
    const meetsAddressPrefRequirement =
        this.autofillSettingsEnterprisePolicyEnabled_ || addressPref.value;
    const meetsAiPrefRequirement =
        autofillAiPref.value !== ModelExecutionEnterprisePolicyValue.DISABLE;

    this.allowNewEntitiesAddition_ = this.isNewEntitiesAdditionAllowedByPref_ &&
        this.canEnableOrDisableAutofillAi_ && meetsAddressPrefRequirement &&
        meetsAiPrefRequirement;
  }

  // Refreshes the entity types list when the sync status changes.
  //
  // Updates the list to reflect whether the user is signed in (allowing the
  // creation of entity instances for types stored on the server) or signed
  // out (disallowing it).
  private onSyncStatusChanged_(_: SyncStatus) {
    this.entityDataManager_.getWritableEntityTypes().then(
        (entityTypes: EntityType[]) => {
          this.updateEntityTypesList_(entityTypes);
        });
  }

  private get isNewEntitiesAdditionAllowedByPref_(): boolean {
    return this.allowNewEntitiesAdditionPref?.value ?? true;
  }

  protected typeNameToIconName_(name: EntityTypeName): string|undefined {
    switch (name) {
      case EntityTypeName.kDriversLicense:
        return 'settings20:id-card';
      case EntityTypeName.kFlightReservation:
        return 'settings20:travel';
      case EntityTypeName.kKnownTravelerNumber:
        return 'privacy20:person-check';
      case EntityTypeName.kNationalIdCard:
        return 'settings20:id-card';
      case EntityTypeName.kOrder:
        return 'settings20:orders';
      case EntityTypeName.kPassport:
        return 'settings20:passport';
      case EntityTypeName.kRedressNumber:
        return 'privacy20:person-check';
      case EntityTypeName.kShipment:
        return 'settings20:local-shipping';
      case EntityTypeName.kVehicle:
        return 'settings20:directions-car';
      default:
        return undefined;
    }
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
