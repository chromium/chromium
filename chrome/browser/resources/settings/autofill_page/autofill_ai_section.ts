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

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
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

// browser_element_identifiers constants
const AUTOFILL_AI_HEADER_ELEMENT_ID =
    'SettingsUI::kAutofillPredictionImprovementsHeaderElementId';

export interface SettingsAutofillAiSectionElement {
  $: {
    actionMenu: CrLazyRenderElement<CrActionMenuElement>,
    entriesHeaderTitle: HTMLElement,
  };
}

const SettingsAutofillAiSectionElementBase =
    HelpBubbleMixin(PrefsMixin(I18nMixin(PolymerElement)));

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
         The correspondent model for any entity related action menus or
         dialogs.
       */
      activeEntity_: {
        type: Object,
        value: null,
      },

      /** The same dialog can be used for both adding and editing entities. */
      showAddOrEditEntityDialog_: {
        type: Boolean,
        value: false,
      },

      showRemoveEntityDialog_: {
        type: Boolean,
        value: false,
      },

      entityInstances_: {
        Array,
        value: () => [],
      },
    };
  }

  ineligibleUser: boolean;
  private activeEntity_: EntityInstance|null;
  private showAddOrEditEntityDialog_: boolean;
  private showRemoveEntityDialog_: boolean;
  private entityInstances_: EntityInstance[];
  private entityDataManager_: EntityDataManagerProxy =
      EntityDataManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.entityDataManager_.loadEntityInstances().then(
        (entityInstances: EntityInstance[]) => {
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

  /**
   * Returns the first attribute of the entity, to be used as a label.
   */
  private getEntityLabel_(entity: EntityInstance): string {
    // TODO(crbug.com/393318914): Use better labels.
    return entity.attributes[0].value;
  }

  /**
   * Returns the text to be used in the "Delete" entity dialog.
   */
  private getRemoveEntityText_(entity: EntityInstance): string {
    // TODO(crbug.com/393319296): Make the string more suggestive.
    return this.i18n(
        'autofillAiDeleteEntryDialogText', this.getEntityLabel_(entity),
        this.getEntityLabel_(entity));
  }

  /**
   * Open the action menu.
   */
  private onMoreButtonClick_(e: DomRepeatEvent<EntityInstance>) {
    this.activeEntity_ = e.model.item;
    const moreButton = e.target as HTMLElement;
    this.$.actionMenu.get().showAt(moreButton);
  }

  /**
   * Handles tapping on the "Add" entity button.
   */
  private onAddEntityClick_(e: Event) {
    e.preventDefault();
    this.showAddOrEditEntityDialog_ = true;
  }

  /**
   * Handles tapping on the "Edit" entity button in the action menu.
   */
  private onMenuEditEntityClick_(e: Event) {
    e.preventDefault();
    // Clone item so dialog won't update model on cancel.
    this.activeEntity_ = structuredClone(this.activeEntity_);
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

  private onRemoveEntityDialogClose_() {
    const wasDeletionConfirmed =
        this.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#removeEntityDialog')!.wasConfirmed();
    if (wasDeletionConfirmed) {
      this.entityDataManager_.removeEntityInstance(this.activeEntity_!.guid);
      // Speculatively update local list to avoid potential stale data issues.
      const deletedEntityIndex = this.entityInstances_.findIndex(
          entityInstance => entityInstance.guid === this.activeEntity_!.guid);
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
