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
import './search_engine_icon.js';

import {ExtensionControlBrowserProxyImpl} from '/shared/settings/extension_control_browser_proxy.js';
import type {ExtensionControlBrowserProxy} from '/shared/settings/extension_control_browser_proxy.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './search_engine_entry.css.js';
import {getHtml} from './search_engine_entry.html.js';
import type {SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {ChoiceMadeLocation, SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from './search_engines_browser_proxy.js';

const SettingsSearchEngineEntryElementBase =
    PrefServiceObserverMixinLit(I18nMixinLit(CrLitElement));

export type SearchEngineEntryElement = SettingsSearchEngineEntryElement;

export class SettingsSearchEngineEntryElement extends
    SettingsSearchEngineEntryElementBase {
  static get is() {
    return 'settings-search-engine-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      engine: {type: Object},
      defaultSearchProviderDataPref_: {type: Object},
      showShortcut: {type: Boolean, reflect: true},
      showQueryUrl: {type: Boolean, reflect: true},
      isDefault: {type: Boolean, reflect: true},
      searchSettingsUpdateEnabled_: {type: Boolean},
    };
  }

  accessor engine: SearchEngine = {
    canBeDefault: false,
    canBeEdited: false,
    canBeRemoved: false,
    canBeActivated: false,
    canBeDeactivated: false,
    default: false,
    displayName: '',
    iconPath: '',
    id: -1,
    isManaged: false,
    isOmniboxExtension: false,
    isPrepopulated: false,
    isStarterPack: false,
    keyword: '',
    name: '',
    shouldConfirmRemoval: false,
    url: '',
    urlLocked: false,
  };
  accessor showShortcut: boolean = false;
  accessor showQueryUrl: boolean = false;
  accessor isDefault: boolean = false;
  protected accessor defaultSearchProviderDataPref_:
      chrome.settingsPrivate.PrefObject|undefined = undefined;
  protected accessor searchSettingsUpdateEnabled_: boolean =
      loadTimeData.getBoolean('searchSettingsUpdate');

  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();
  private extensionBrowserProxy_: ExtensionControlBrowserProxy =
      ExtensionControlBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(
        'default_search_provider_data.template_url_data',
        'defaultSearchProviderDataPref_');
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('engine')) {
      this.isDefault = this.engine.default;
    }
  }

  private closePopupMenu_() {
    this.shadowRoot.querySelector('cr-action-menu')!.close();
  }

  private isDefaultEngineManagedByExtension_(): boolean {
    if (!this.engine.extension || this.engine.isOmniboxExtension) {
      return false;
    }

    const extensionId = this.defaultSearchProviderDataPref_?.extensionId;
    return !!extensionId && extensionId === this.engine.extension.id;
  }

  protected showEditOption_(): boolean {
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

  protected shouldShowEditIcon_(): boolean {
    return !this.searchSettingsUpdateEnabled_ && this.showEditOption_() &&
        !this.engine.canBeActivated;
  }

  protected shouldShowSecondaryButton_(): boolean {
    return !this.engine.canBeActivated &&
        (this.engine.isManaged && !this.engine.canBeEdited);
  }

  protected shouldDisableDots_(): boolean {
    // Disable the dots if none of the options are available for the engine.
    if (this.searchSettingsUpdateEnabled_) {
      if (this.isDefaultEngineManagedByExtension_()) {
        return true;
      }

      return !this.showEditOption_() && !this.engine.canBeActivated &&
          !this.engine.canBeDeactivated && !this.engine.canBeRemoved &&
          !this.engine.canBeDefault;
    }

    return this.engine.default ||
        (this.engine.isManaged && !this.engine.canBeActivated &&
         !this.engine.canBeDeactivated && !this.engine.canBeRemoved);
  }

  protected turnOnLabel_(): string {
    return this.engine.extension ? this.i18n('searchActivateShortcut') :
                                   this.i18n('searchActivate');
  }

  protected turnOffLabel_(): string {
    return this.engine.extension ? this.i18n('searchDeactivateShortcut') :
                                   this.i18n('searchDeactivate');
  }

  protected showDeactivateOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);
    // `canBeDeactivated` is always false if the engine is the current default,
    // but it should be shown (and disabled) anyway. Hide the deactivate option
    // if the engine is prepopulated, as the user should not be able to turn it
    // off.
    return this.engine.canBeDeactivated ||
        (this.engine.default && !this.engine.isPrepopulated);
  }

  protected showDeleteOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);
    // `canBeRemoved` is always false if the engine is the current default,
    // but it should be shown (and disabled) anyway.
    return this.engine.canBeRemoved || this.engine.default;
  }

  protected showMakeDefaultOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);
    // Hide the make default option for starter pack and extension shortcuts,
    // except if they are the current default (e.g. by policy).
    return !this.engine.isStarterPack &&
        (!this.engine.extension || this.engine.default);
  }

  protected showDisableExtensionOption_(): boolean {
    assert(this.searchSettingsUpdateEnabled_);
    return this.engine.isOmniboxExtension && !!this.engine.extension &&
        this.engine.extension?.canBeDisabled;
  }

  protected showControlledIndicator_(): boolean {
    return !this.searchSettingsUpdateEnabled_ && !!this.engine.extension;
  }

  protected onManageClick_() {
    assert(this.engine.extension);
    this.closePopupMenu_();
    this.browserProxy_.recordSearchEnginesPageHistogram(
        SearchEnginesInteractions.EXTENSION_MANAGE);
    this.extensionBrowserProxy_.manageExtension(this.engine.extension.id);
  }

  protected onDisableClick_() {
    assert(this.engine.extension);
    assert(this.engine.extension.canBeDisabled);
    this.browserProxy_.recordSearchEnginesPageHistogram(
        SearchEnginesInteractions.EXTENSION_DISABLE);
    this.extensionBrowserProxy_.disableExtension(this.engine.extension.id);
  }

  protected onDeleteClick_(e: Event) {
    e.preventDefault();
    this.closePopupMenu_();

    if (!this.engine.shouldConfirmRemoval) {
      this.browserProxy_.removeSearchEngine(this.engine.id);
      return;
    }

    const dots = this.shadowRoot.querySelector('cr-icon-button.icon-more-vert');
    assert(dots);

    this.fire('delete-search-engine', {
      engine: this.engine,
      anchorElement: dots,
    });
  }

  protected onDotsClick_() {
    this.browserProxy_.recordSearchEnginesPageHistogram(
        SearchEnginesInteractions.MORE_ACTIONS);
    const dots = this.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assert(dots);
    this.shadowRoot.querySelector('cr-action-menu')!.showAt(dots, {
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  }

  protected onViewOrEditClick_(e: Event) {
    e.preventDefault();
    this.closePopupMenu_();

    // Only record an edit event if the engine is modifiable.
    if (!this.shouldShowSecondaryButton_()) {
      this.browserProxy_.recordSearchEnginesPageHistogram(
          SearchEnginesInteractions.EDIT_SEARCH_ENGINE);
    }

    const anchorToActionMenu =
        this.searchSettingsUpdateEnabled_ && !this.shouldShowSecondaryButton_();
    const anchor = this.shadowRoot.querySelector(
        anchorToActionMenu ? 'cr-icon-button.icon-more-vert' :
                             'cr-icon-button');
    assert(anchor);

    this.fire('view-or-edit-search-engine', {
      engine: this.engine,
      anchorElement: anchor,
    });
  }

  protected onMakeDefaultClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(
        this.engine.id, ChoiceMadeLocation.SEARCH_ENGINE_SETTINGS,
        /*saveGuestChoice=*/ null);
  }

  protected onActivateClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.id, /*is_active=*/ true);
  }

  protected onDeactivateClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.id, /*is_active=*/ false);
  }

  protected getMoreActionsAriaLabel_(): string {
    return this.i18n(
        'searchEnginesMoreActionsAriaLabel', this.engine.displayName);
  }

  protected getActivateButtonAriaLabel_(): string {
    return this.i18n(
        'searchEnginesActivateButtonAriaLabel', this.engine.displayName);
  }

  protected getEditButtonAriaLabel_(): string {
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
