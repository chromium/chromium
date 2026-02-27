// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {SpHeadingElement} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Action, Category} from '../customize_toolbar.mojom-webui.js';

import {CustomizeToolbarApiProxy} from './customize_toolbar_api_proxy.js';
import {getCss} from './toolbar.css.js';
import {getHtml} from './toolbar.html.js';

const ToolbarElementBase = WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export interface ToolbarElement {
  $: {
    heading: SpHeadingElement,
    pinningSelectionCard: HTMLElement,
  };
}

export class ToolbarElement extends ToolbarElementBase {
  static get is() {
    return 'customize-chrome-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      actions_: {type: Array},
      categories_: {type: Array},
      resetToDefaultDisabled_: {type: Boolean},
    };
  }

  private apiProxy_: CustomizeToolbarApiProxy =
      CustomizeToolbarApiProxy.getInstance();
  private listenerIds_: number[] = [];

  protected accessor actions_: Action[] = [];
  protected accessor categories_: Category[] = [];
  protected accessor resetToDefaultDisabled_: boolean = true;

  constructor() {
    super();
    this.populateUi_();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.apiProxy_.callbackRouter.setActionPinned.addListener(
          this.setActionPinned_.bind(this)),
      this.apiProxy_.callbackRouter.notifyActionsUpdated.addListener(
          this.populateUi_.bind(this)),
    ];

    this.addWebUiListener('theme-changed', this.populateUi_.bind(this));
    chrome.send('observeThemeChanges');
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  protected onBackButtonClick_() {
    this.fire('back-click');
  }

  protected onResetToDefaultClick_() {
    this.apiProxy_.handler.resetToDefault();
    const announcer = getAnnouncerInstance();
    announcer.announce(this.i18n('resetToDefaultButtonAnnouncement'));
  }

  protected getActionToggleHandler_(actionId: number, nextValue: boolean) {
    return (_event: CustomEvent<boolean>) =>
               this.apiProxy_.handler.pinAction(actionId, nextValue);
  }

  private setActionPinned_(actionId: number, pinned: boolean) {
    this.actions_ = this.actions_.map((action) => {
      if (action.id === actionId) {
        action.pinned = pinned;
      }

      return action;
    });

    this.updateResetToDefaultDisabled();
  }

  private populateUi_() {
    this.apiProxy_.handler.listActions().then(({actions}) => {
      this.actions_ = actions;
      assert(this.actions_.every(action => action.iconUrl.startsWith('data:')));
    });

    this.apiProxy_.handler.listCategories().then(({categories}) => {
      this.categories_ = categories;
    });

    this.updateResetToDefaultDisabled();
  }

  private updateResetToDefaultDisabled() {
    this.apiProxy_.handler.getIsCustomized().then(({customized}) => {
      this.resetToDefaultDisabled_ = !customized;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-toolbar': ToolbarElement;
  }
}

customElements.define(ToolbarElement.is, ToolbarElement);
