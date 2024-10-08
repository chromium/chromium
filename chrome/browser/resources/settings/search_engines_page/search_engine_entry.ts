// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import './search_engine_entry.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './search_engine_entry.html.js';
import type {SearchEngine, SearchEnginesBrowserProxy} from './search_engines_browser_proxy.js';
import {ChoiceMadeLocation, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineEntryElement {
  $: {
    delete: HTMLButtonElement,
    makeDefault: HTMLButtonElement,
    edit: HTMLButtonElement,
  };
}

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
    };
  }

  engine: SearchEngine;
  showShortcut: boolean;
  showQueryUrl: boolean;
  isDefault: boolean;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();
  private showEditIcon_: boolean;

  private closePopupMenu_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  private computeIsDefault_(): boolean {
    return this.engine.default;
  }

  private computeShowEditIcon_(): boolean {
    return !this.engine.isStarterPack && !this.engine.canBeActivated &&
        !this.engine.isManaged;
  }

  private onDeleteClick_(e: Event) {
    e.preventDefault();
    this.closePopupMenu_();

    if (!this.engine.shouldConfirmDeletion) {
      this.browserProxy_.removeSearchEngine(this.engine.modelIndex);
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

  private onMakeDefaultClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(
        this.engine.modelIndex, ChoiceMadeLocation.SEARCH_ENGINE_SETTINGS,
        /*saveGuestChoice=*/ null);
  }

  private onActivateClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.modelIndex, /*is_active=*/ true);
  }

  private onDeactivateClick_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.modelIndex, /*is_active=*/ false);
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
