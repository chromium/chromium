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
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import {getTemplate} from './autofill_ai_section.html.js';
import type {EntityDataManagerProxy} from './entity_data_manager_proxy.js';
import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';

type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;
type EntityType = chrome.autofillPrivate.EntityType;

// browser_element_identifiers constants
const AUTOFILL_AI_HEADER_ELEMENT_ID =
    'SettingsUI::kAutofillPredictionImprovementsHeaderElementId';

export interface SettingsAutofillAiSectionElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    addMenu: CrLazyRenderElement<CrActionMenuElement>,
    entriesHeaderTitle: HTMLElement,
  };
}

const SettingsAutofillAiSectionElementBase =
    HelpBubbleMixin(PrefsMixin(PolymerElement));

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
         The corresponding `EntityInstance` model for any entity related action
         menus or dialogs.
       */
      activeEntity_: {
        type: Object,
        value: null,
      },

      /**
         Complete list of entities that exist. When the user wants to add a new
         entity, this list is displayed.
       */
      completeEntityList_: {
        type: Array,
        value: () => [],
      },

      /** The same dialog can be used for both adding and editing entities. */
      showAddOrEditEntityDialog_: {
        type: Boolean,
        value: false,
      },

      addOrEditEntityDialogTitle_: {
        type: String,
        value: '',
      },

      showRemoveEntityDialog_: {
        type: Boolean,
        value: false,
      },

      entityInstances_: {
        type: Array,
        value: () => [],
      },
    };
  }

  ineligibleUser: boolean;
  private activeEntity_: EntityInstance|null;
  private completeEntityList_: EntityType[];
  private showAddOrEditEntityDialog_: boolean;
  private addOrEditEntityDialogTitle_: string;
  private showRemoveEntityDialog_: boolean;
  private entityInstances_: EntityInstanceWithLabels[];

  // The correspondent `EntityInstanceWithLabels` model for any entity related
  // action menus or dialogs.
  private activeEntityWithLabels_: EntityInstanceWithLabels|null;
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.entityDataManager_.getAllEntityTypes().then(
        (entityTypes: EntityType[]) => {
          this.completeEntityList_ = entityTypes;
        });

    this.entityDataManager_.loadEntityInstances().then(
        (entityInstances: EntityInstanceWithLabels[]) => {
          // If the user is ineligible for Autofill with Ai and has no data
          // saved, then they should not be able to access this page. These
          // lines prevent such a user manually navigating to this page by
          // typing its URL.
          if (this.ineligibleUser && entityInstances.length === 0) {
            Router.getInstance().navigateTo(routes.AUTOFILL);
            return;
          }
          this.entityInstances_ = entityInstances;
        });

    // TODO(crbug.com/393318914): Remove this help bubble, which was introduced
    // in crrev.com/c/5939704.
    this.registerHelpBubble(
        AUTOFILL_AI_HEADER_ELEMENT_ID, this.$.entriesHeaderTitle);
  }

  private onToggleSubLabelLinkClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('autofillAiLearnMoreURL'));
  }

  private computeDisableAddButton_(
      ineligibleUser: boolean, optInPrefValue: boolean): boolean {
    return ineligibleUser || !optInPrefValue;
  }

  /**
   * Open the action menu.
   */
  private onMoreButtonClick_(e: DomRepeatEvent<EntityInstanceWithLabels>) {
    this.activeEntityWithLabels_ = e.model.item;
    const moreButton = e.target as HTMLElement;
    this.$.actionMenu.get().showAt(moreButton);
  }

  /**
   * Handles tapping on the "Add" entity button.
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
    // Create a new entity with no attributes and guid. A guid will be assigned
    // after saving, on the C++ side.
    this.activeEntity_ = {
      type: e.model.item,
      attributes: [],
      guid: '',
      nickname: '',
    };
    this.addOrEditEntityDialogTitle_ = this.activeEntity_.type.addEntityString;
    this.showAddOrEditEntityDialog_ = true;
    this.$.addMenu.get().close();
  }

  /**
   * Handles tapping on the "Edit" entity button in the action menu.
   */
  private async onMenuEditEntityClick_(e: Event) {
    e.preventDefault();
    this.activeEntity_ = await this.entityDataManager_.getEntityInstanceByGuid(
        this.activeEntityWithLabels_!.guid);
    this.addOrEditEntityDialogTitle_ = this.activeEntity_.type.editEntityString;
    this.showAddOrEditEntityDialog_ = true;
    this.$.actionMenu.get().close();
  }

  /**
   * Handles tapping on the "Delete" entity button in the action menu.
   */
  private onMenuRemoveEntityClick_(e: Event) {
    e.preventDefault();
    this.showRemoveEntityDialog_ = true;
    this.$.actionMenu.get().close();
  }

  private onAutofillAiAddOrEditDone_(e: CustomEvent<EntityInstance>) {
    e.stopPropagation();
    this.entityDataManager_.addOrUpdateEntityInstance(e.detail);
  }

  private onAddOrEditEntityDialogClose_(e: Event) {
    e.stopPropagation();
    this.showAddOrEditEntityDialog_ = false;
  }

  private onRemoveEntityDialogClose_() {
    const wasDeletionConfirmed =
        this.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#removeEntityDialog')!.wasConfirmed();
    if (wasDeletionConfirmed) {
      this.entityDataManager_.removeEntityInstance(
          this.activeEntityWithLabels_!.guid);
      // Speculatively update local list to avoid potential stale data issues.
      const deletedEntityIndex = this.entityInstances_.findIndex(
          entityInstance =>
              entityInstance.guid === this.activeEntityWithLabels_!.guid);
      this.splice('entityInstances_', deletedEntityIndex, 1);
    }

    this.showRemoveEntityDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-ai-section': SettingsAutofillAiSectionElement;
  }
}

customElements.define(
    SettingsAutofillAiSectionElement.is, SettingsAutofillAiSectionElement);
