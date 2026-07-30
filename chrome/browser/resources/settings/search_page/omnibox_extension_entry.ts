// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-omnibox-extension-entry' is a component for showing
 * an omnibox extension with its name and keyword.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../site_favicon.js';

import type {ExtensionControlBrowserProxy} from '/shared/settings/extension_control_browser_proxy.js';
import {ExtensionControlBrowserProxyImpl} from '/shared/settings/extension_control_browser_proxy.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {FocusRowMixinLit} from 'chrome://resources/cr_elements/focus_row_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './omnibox_extension_entry.css.js';
import {getHtml} from './omnibox_extension_entry.html.js';
import type {SearchEngine} from './search_engines_browser_proxy.js';

export interface SettingsOmniboxExtensionEntryElement {
  $: {
    disable: HTMLButtonElement,
    manage: HTMLButtonElement,
  };
}

const SettingsOmniboxExtensionEntryElementBase = FocusRowMixinLit(CrLitElement);

export class SettingsOmniboxExtensionEntryElement extends
    SettingsOmniboxExtensionEntryElementBase {
  static get is() {
    return 'settings-omnibox-extension-entry';
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
  private browserProxy_: ExtensionControlBrowserProxy =
      ExtensionControlBrowserProxyImpl.getInstance();

  protected onManageClick_() {
    this.closePopupMenu_();
    this.browserProxy_.manageExtension(this.engine.extension!.id);
  }

  protected onDisableClick_() {
    this.closePopupMenu_();
    this.browserProxy_.disableExtension(this.engine.extension!.id);
  }

  private closePopupMenu_() {
    this.shadowRoot.querySelector('cr-action-menu')!.close();
  }

  protected onDotsClick_() {
    const dots = this.shadowRoot.querySelector('cr-icon-button');
    assert(dots);
    this.shadowRoot.querySelector('cr-action-menu')!.showAt(dots, {
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-omnibox-extension-entry': SettingsOmniboxExtensionEntryElement;
  }
}

customElements.define(
    SettingsOmniboxExtensionEntryElement.is,
    SettingsOmniboxExtensionEntryElement);
