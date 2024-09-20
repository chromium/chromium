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
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './startup_url_entry.html.js';
import type {StartupPageInfo} from './startup_urls_page_browser_proxy.js';
import {StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';

/**
 * The name of the event fired from this element when the "Edit" option is
 * clicked.
 */
export const EDIT_STARTUP_URL_EVENT: string = 'edit-startup-url';

const SettingsStartupUrlEntryElementBase = FocusRowMixin(PolymerElement);

export class SettingsStartupUrlEntryElement extends
    SettingsStartupUrlEntryElementBase {
  static get is() {
    return 'settings-startup-url-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      editable: {
        type: Boolean,
        reflectToAttribute: true,
      },

      model: Object,
    };
  }

  editable: boolean;
  model: StartupPageInfo;

  private onRemoveClick_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  }

  private onEditClick_(e: Event) {
    e.preventDefault();
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.dispatchEvent(new CustomEvent(EDIT_STARTUP_URL_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        model: this.model,
        anchor: this.shadowRoot!.querySelector('#dots'),
      },
    }));
  }

  private onDotsClick_() {
    const actionMenu =
        this.shadowRoot!
            .querySelector<CrLazyRenderElement<CrActionMenuElement>>(
                '#menu')!.get();
    const dots = this.shadowRoot!.querySelector<HTMLElement>('#dots');
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
