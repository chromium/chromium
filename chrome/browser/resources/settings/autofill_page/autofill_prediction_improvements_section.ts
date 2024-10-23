// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-prediction-improvements-section' is
 * the section containing configuration options for prediction improvements.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
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
import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import {getTemplate} from './autofill_prediction_improvements_section.html.js';
import type {UserAnnotationsManagerProxy} from './user_annotations_manager_proxy.js';
import {UserAnnotationsManagerProxyImpl} from './user_annotations_manager_proxy.js';

type UserAnnotationsEntry = chrome.autofillPrivate.UserAnnotationsEntry;

// browser_element_identifiers constants
const PREDICTION_IMPROVEMENTS_HEADER_ELEMENT_ID =
    'SettingsUI::kAutofillPredictionImprovementsHeaderElementId';

export interface SettingsAutofillPredictionImprovementsSectionElement {
  $: {
    prefToggle: SettingsToggleButtonElement,
    entriesHeaderTitle: HTMLElement,
  };
}

const SettingsAutofillPredictionImprovementsSectionElementBase =
    HelpBubbleMixin(PrefsMixin(I18nMixin(PolymerElement)));

export class SettingsAutofillPredictionImprovementsSectionElement extends
    SettingsAutofillPredictionImprovementsSectionElementBase {
  static get is() {
    return 'settings-autofill-prediction-improvements-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        reflectToAttribute: true,
      },

      entryToDelete_: Object,

      deleteEntryConfirmationText_: {
        type: String,
        computed: 'getDeleteEntryConfirmationText_(entryToDelete_)',
      },

      deleteAllEntriesConfirmationShown_: {
        type: Boolean,
        value: false,
      },
    };
  }

  disabled: boolean = false;
  private userAnnotationsEntries_: UserAnnotationsEntry[] = [];
  private userAnnotationsManager_: UserAnnotationsManagerProxy =
      UserAnnotationsManagerProxyImpl.getInstance();
  private entryToDelete_?: UserAnnotationsEntry;
  private deleteEntryConfirmationText_: string;
  private deleteAllEntriesConfirmationShown_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    this.userAnnotationsManager_.getEntries().then(
        (entries: UserAnnotationsEntry[]) => {
          if (this.disabled && entries.length === 0) {
            Router.getInstance().navigateTo(routes.AUTOFILL);
          }
          this.userAnnotationsEntries_ = entries;
        });

    this.registerHelpBubble(
        PREDICTION_IMPROVEMENTS_HEADER_ELEMENT_ID, this.$.entriesHeaderTitle);
  }

  private onToggleSubLabelLinkClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('autofillPredictionImprovementsLearnMoreURL'));
  }

  private onPrefToggleChanged_() {
    this.userAnnotationsManager_.predictionImprovementsIphFeatureUsed();

    this.maybeTriggerBootstrapping_();
  }

  private async maybeTriggerBootstrapping_() {
    const bootstrappingDisabled =
        !loadTimeData.getBoolean('autofillPredictionBootstrappingEnabled');
    const toggleDisabled = !this.$.prefToggle.checked;
    const hasEntries = await this.userAnnotationsManager_.hasEntries();
    // Only trigger bootstrapping if the pref was just enabled and there are no
    // entries yet.
    if (bootstrappingDisabled || this.disabled || toggleDisabled ||
        hasEntries) {
      return;
    }

    const entriesAdded =
        await this.userAnnotationsManager_.triggerBootstrapping();
    // Refresh the list if bootstrapping resulted in new entries being added.
    if (entriesAdded) {
      this.userAnnotationsEntries_ =
          await this.userAnnotationsManager_.getEntries();
    }
  }

  private onDeleteEntryCick_(e: DomRepeatEvent<UserAnnotationsEntry>): void {
    this.entryToDelete_ = e.model.item;
  }

  private onDeleteEntryDialogClose_(): void {
    assert(this.entryToDelete_);

    const wasDeletionConfirmed =
        this.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#deleteEntryDialog')!.wasConfirmed();

    if (wasDeletionConfirmed) {
      this.userAnnotationsManager_.deleteEntry(this.entryToDelete_.entryId);

      // Speculatively update local list to avoid potential stale data issues.
      const index = this.userAnnotationsEntries_.findIndex(
          entry => this.entryToDelete_!.entryId === entry.entryId);
      this.splice('userAnnotationsEntries_', index, 1);
    }

    this.entryToDelete_ = undefined;
  }

  private onDeleteAllEntriesClick_(): void {
    this.deleteAllEntriesConfirmationShown_ = true;
  }

  private onDeleteAllEntriesDialogClose_(): void {
    const wasDeletionConfirmed =
        this.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#deleteAllEntriesDialog')!.wasConfirmed();
    if (wasDeletionConfirmed) {
      this.userAnnotationsManager_.deleteAllEntries();
      this.userAnnotationsEntries_ = [];
    }

    this.deleteAllEntriesConfirmationShown_ = false;
  }

  private getDeleteEntryConfirmationText_(entry?: UserAnnotationsEntry):
      string {
    if (!entry) {
      return '';
    }
    return this.i18n(
        'autofillPredictionImprovementsDeleteEntryDialogText', entry.key,
        entry.value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-prediction-improvements-section':
        SettingsAutofillPredictionImprovementsSectionElement;
  }
}

customElements.define(
    SettingsAutofillPredictionImprovementsSectionElement.is,
    SettingsAutofillPredictionImprovementsSectionElement);
