// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-startup-url-entry represents a UI component that
 * displays a URL that is loaded during startup. It includes a menu that allows
 * the user to edit/remove the entry.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../site_favicon.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './startup_url_entry.css.js';
import {getHtml} from './startup_url_entry.html.js';
import type {StartupPageInfo} from './startup_urls_page_browser_proxy.js';
import {StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';

/**
 * The name of the event fired from this element when the "Edit" option is
 * clicked.
 */
export const EDIT_STARTUP_URL_EVENT: string = 'edit-startup-url';

const SettingsStartupUrlEntryElementBase = I18nMixinLit(CrLitElement);

export class SettingsStartupUrlEntryElement extends
    SettingsStartupUrlEntryElementBase {
  static get is() {
    return 'settings-startup-url-entry';
  }

  static override get shadowRootOptions() {
    return {...super.shadowRootOptions, delegatesFocus: true};
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      editable: {
        type: Boolean,
        reflect: true,
      },

      model: {type: Object},
    };
  }

  accessor editable: boolean = false;
  accessor model: StartupPageInfo;

  protected getMoreActionsTitle_(): string {
    return this.i18n('moreActionsFor', this.model.title);
  }

  protected onRemoveClick_() {
    this.shadowRoot.querySelector('cr-action-menu')!.close();
    StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  }

  protected onEditClick_(e: Event) {
    e.preventDefault();
    this.shadowRoot.querySelector('cr-action-menu')!.close();
    this.fire(EDIT_STARTUP_URL_EVENT, {
      model: this.model,
      anchor: this.shadowRoot.querySelector('#dots'),
    });
  }

  protected onDotsClick_() {
    const actionMenu =
        this.shadowRoot
            .querySelector<CrLazyRenderLitElement<CrActionMenuElement>>(
                '#menu')!.get();
    const dots = this.shadowRoot.querySelector<HTMLElement>('#dots');
    assert(dots);
    actionMenu.showAt(dots);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-startup-url-entry': SettingsStartupUrlEntryElement;
  }
}

customElements.define(
    SettingsStartupUrlEntryElement.is, SettingsStartupUrlEntryElement);
