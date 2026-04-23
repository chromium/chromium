// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './search_engine_entry.css.js';
import '../settings_shared.css.js';
import './search_engine_icon.js';

import {ExtensionControlBrowserProxyImpl} from '/shared/settings/extension_control_browser_proxy.js';
import type {ExtensionControlBrowserProxy} from '/shared/settings/extension_control_browser_proxy.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './search_engine_entry.html.js';
import type {SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {ChoiceMadeLocation, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

const SettingsSearchEngineEntryElementBase = I18nMixin(PolymerElement);

export class SettingsSearchEngineEntryElement extends
    SettingsSearchEngineEntryElementBase {
  static get is() {
    return 'settings-search-engine-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      engine: Object,

      showShortcut: {type: Boolean, value: false, reflectToAttribute: true},

      showQueryUrl: {type: Boolean, value: false, reflectToAttribute: true},

      isDefault: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsDefault_(engine)',
      },

      showEditIcon_: {
        type: Boolean,
        computed: 'computeShowEditIcon_(engine)',
      },

      showSecondaryButton_: {
        type: Boolean,
        computed: 'computeShowSecondaryButton_(engine)',
      },

      disableDots_: {
        type: Boolean,
        computed: 'computeDisableDots_(engine)',
      },

      turnOnLabel: {
        type: String,
        computed: 'computeTurnOnLabel_(engine)',
      },

      turnOffLabel: {
        type: String,
        computed: 'computeTurnOffLabel_(engine)',
      },

      searchSettingsUpdateEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchSettingsUpdate'),
      },
    };
  }

  declare engine: SearchEngine;
  declare showShortcut: boolean;
  declare showQueryUrl: boolean;
  declare isDefault: boolean;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();
  private extensionBrowserProxy_: ExtensionControlBrowserProxy =
      ExtensionControlBrowserProxyImpl.getInstance();
  declare private showEditIcon_: boolean;
  declare private showSecondaryButton_: boolean;
  declare private disableDots_: boolean;
  declare turnOnLabel: string;
  declare turnOffLabel: string;

  declare private searchSettingsUpdateEnabled_: boolean;

  private closePopupMenu_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  private showEditOption_(): boolean {
    // Hide the edit option for extension shortcuts except if they are the
    // current default (e.g. by policy).
    if (this.searchSettingsUpdateEnabled_ && this.engine.extension &&
        !this.engine.default) {
      return false;
    }

    if (this.engine.isStarterPack) {
      return false;
    }

    if (this.engine.canBeEdited) {
      return true;
    }

    return !this.engine.isManaged;
  }

  private computeIsDefault_(): boolean {
    return this.engine.default;
  }

  private computeShowEditIcon_(): boolean {
    return !this.searchSettingsUpdateEnabled_ && this.showEditOption_() &&
        !this.engine.canBeActivated;
  }

  private computeShowSecondaryButton_(): boolean {
    return !this.engine.canBeActivated &&
        (this.engine.isManaged && !this.engine.canBeEdited);
  }

  private computeDisableDots_(): boolean {
    // Disable the dots if none of the options are available for the engine.
    if (this.searchSettingsUpdateEnabled_) {
      return !this.showEditOption_() && !this.engine.canBeActivated &&
          !this.engine.canBeDeactivated && !this.engine.canBeRemoved &&
          !this.engine.canBeDefault;
    }

    return this.engine.default ||
        (this.engine.isManaged && !this.engine.canBeActivated &&
         !this.engine.canBeDeactivated && !this.engine.canBeRemoved);
  }

  private computeTurnOnLabel_(): string {
    return this.engine.extension ? this.i18n('searchActivateShortcut') :
                                   this.i18n('searchActivate');
  }

  private computeTurnOffLabel_(): string {
    return this.engine.extension ? this.i18n('searchDeactivateShortcut') :
                                   this.i18n('searchDeactivate');
  }

  private showDeactivateOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);

    // `canBeDeactivated` is always false if the engine is the current default,
    // but it should be shown (and disabled) anyway. Hide the deactivate option
    // if the engine is prepopulated, as the user should not be able to turn it
    // off.
    return this.engine.canBeDeactivated ||
        (this.engine.default && !this.engine.isPrepopulated);
  }

  private showDeleteOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);

    // `canBeRemoved` is always false if the engine is the current default,
    // but it should be shown (and disabled) anyway. Hide the delete option if
    // the engine is prepopulated, as the user should not be able to delete it.
    return this.engine.canBeRemoved ||
        (this.engine.default && !this.engine.isPrepopulated);
  }

  private showMakeDefaultOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);

    // Hide the make default option for starter pack and extension shortcuts,
    // except if they are the current default (e.g. by policy).
    return !this.engine.isStarterPack &&
        (!this.engine.extension || this.engine.default);
  }

  private showDisableExtensionOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);
    return !!this.engine.extension && this.engine.extension.canBeDisabled;
  }

  private showControlledIndicator_(): boolean {
    return !this.searchSettingsUpdateEnabled_ && !!this.engine.extension;
  }

  private onManageClick_() {
    assert(this.engine.extension);
    this.closePopupMenu_();
    this.extensionBrowserProxy_.manageExtension(this.engine.extension.id);
  }

  private onDisableClick_() {
    assert(this.engine.extension);
    assert(this.engine.extension.canBeDisabled);
    this.extensionBrowserProxy_.disableExtension(this.engine.extension.id);
  }

  private onDeleteClick_(e: Event) {
    e.preventDefault();
    this.closePopupMenu_();

    if (!this.engine.shouldConfirmRemoval) {
      this.browserProxy_.removeSearchEngine(this.engine.id);
      return;
    }

    const dots =
        this.shadowRoot!.querySelector('cr-icon-button.icon-more-vert');
    assert(dots);

    this.dispatchEvent(new CustomEvent('delete-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine: this.engine,
        anchorElement: dots,
      },
    }));
  }

  private onDotsClick_() {
    const dots = this.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assert(dots);
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(dots, {
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  }

  private onViewOrEditClick_(e: Event) {
    e.preventDefault();
    this.closePopupMenu_();
    const anchor = this.shadowRoot!.querySelector('cr-icon-button');
    assert(anchor);
    this.dispatchEvent(new CustomEvent('view-or-edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine: this.engine,
        anchorElement: anchor,
      },
    }));
  }

  private onEditClick_(e: Event) {
    assert(this.searchSettingsUpdateEnabled_);
    e.preventDefault();
    this.closePopupMenu_();

    const dots =
        this.shadowRoot!.querySelector('cr-icon-button.icon-more-vert');
    assert(dots);

    this.dispatchEvent(new CustomEvent('view-or-edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine: this.engine,
        anchorElement: dots,
      },
    }));
  }

  private onMakeDefaultClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(
        this.engine.id, ChoiceMadeLocation.SEARCH_ENGINE_SETTINGS,
        /*saveGuestChoice=*/ null);
  }

  private onActivateClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.id, /*is_active=*/ true);
  }

  private onDeactivateClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.id, /*is_active=*/ false);
  }

  private getMoreActionsAriaLabel_(): string {
    return this.i18n(
        'searchEnginesMoreActionsAriaLabel', this.engine.displayName);
  }

  private getActivateButtonAriaLabel_(): string {
    return this.i18n(
        'searchEnginesActivateButtonAriaLabel', this.engine.displayName);
  }

  private getEditButtonAriaLabel_(): string {
    return this.i18n(
        'searchEnginesEditButtonAriaLabel', this.engine.displayName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-entry': SettingsSearchEngineEntryElement;
  }
}

customElements.define(
    SettingsSearchEngineEntryElement.is, SettingsSearchEngineEntryElement);
