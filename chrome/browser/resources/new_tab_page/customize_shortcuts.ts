// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './mini_page.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from './i18n_setup.js';
import {CustomizeDialogAction, PageHandlerRemote} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';

/** Element that lets the user configure shortcut settings. */
class CustomizeShortcutsElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-customize-shortcuts';
  }

  static get properties() {
    return {
      customLinksEnabled_: Boolean,
      hide_: Boolean,
    };
  }

  private customLinksEnabled_: boolean;
  private hide_: boolean;

  private pageHandler_: PageHandlerRemote;

  constructor() {
    super();
    const {handler} = NewTabPageProxy.getInstance();
    this.pageHandler_ = handler;
    this.pageHandler_.getMostVisitedSettings().then(
        ({customLinksEnabled, shortcutsVisible}) => {
          this.customLinksEnabled_ = customLinksEnabled;
          this.hide_ = !shortcutsVisible;
        });
  }

  connectedCallback() {
    super.connectedCallback();
    FocusOutlineManager.forDocument(document);
  }

  apply() {
    this.pageHandler_.setMostVisitedSettings(
        this.customLinksEnabled_, /* shortcutsVisible= */ !this.hide_);
  }

  private getCustomLinksAriaPressed_(): string {
    return !this.hide_ && this.customLinksEnabled_ ? 'true' : 'false';
  }

  private getCustomLinksSelected_(): string {
    return !this.hide_ && this.customLinksEnabled_ ? 'selected' : '';
  }

  private getHideClass_(): string {
    return this.hide_ ? 'selected' : '';
  }

  private getMostVisitedAriaPressed_(): string {
    return !this.hide_ && !this.customLinksEnabled_ ? 'true' : 'false';
  }

  private getMostVisitedSelected_(): string {
    return !this.hide_ && !this.customLinksEnabled_ ? 'selected' : '';
  }

  private onCustomLinksClick_() {
    if (!this.customLinksEnabled_) {
      this.pageHandler_.onCustomizeDialogAction(
          CustomizeDialogAction.kShortcutsCustomLinksClicked);
    }
    this.customLinksEnabled_ = true;
    this.hide_ = false;
  }

  private onHideChange_(e: CustomEvent<boolean>) {
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kShortcutsVisibilityToggleClicked);
    this.hide_ = e.detail;
  }

  private onMostVisitedClick_() {
    if (this.customLinksEnabled_) {
      this.pageHandler_.onCustomizeDialogAction(
          CustomizeDialogAction.kShortcutsMostVisitedClicked);
    }
    this.customLinksEnabled_ = false;
    this.hide_ = false;
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-customize-shortcuts': CustomizeShortcutsElement;
  }
}

customElements.define(CustomizeShortcutsElement.is, CustomizeShortcutsElement);
