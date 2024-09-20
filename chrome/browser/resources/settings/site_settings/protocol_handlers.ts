// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'protocol-handlers' is the polymer element for showing the
 * protocol handlers category under Site Settings.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../privacy_icons.html.js';
import '../privacy_page/collapse_radio_button.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './protocol_handlers.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface HandlerEntry {
  host: string;
  is_default: boolean;
  protocol: string;
  protocol_display_name: string;
  spec: string;
}

export interface ProtocolEntry {
  handlers: HandlerEntry[];
  protocol: string;
  protocol_display_name: string;
}

export interface AppHandlerEntry {
  host: string;
  protocol: string;
  protocol_display_name: string;
  spec: string;
  app_id: string;
}

export interface AppProtocolEntry {
  handlers: AppHandlerEntry[];
  protocol: string;
  protocol_display_name: string;
}

export interface ProtocolHandlersElement {
  $: {
    defaultButton: HTMLButtonElement,
  };
}

const ProtocolHandlersElementBase =
    WebUiListenerMixin(SiteSettingsMixin(PolymerElement));

export class ProtocolHandlersElement extends ProtocolHandlersElementBase {
  static get is() {
    return 'protocol-handlers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Array of protocols and their handlers.
       */
      protocols: Array,

      /**
       * Array of allowed app protocols and their handlers.
       */
      appAllowedProtocols: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Array of disallowed app protocols and their handlers.
       */
      appDisallowedProtocols: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Used to determine if the apps title should be shown.
       */
      showAppsProtocolHandlersTitle_: {
        type: Boolean,
        value: false,
      },

      /**
       * The targeted object for menu operations.
       */
      actionMenuModel_: Object,

      /* Labels for the toggle on/off positions. */
      toggleOffLabel: String,
      toggleOnLabel: String,

      /**
       * Array of ignored (blocked) protocols.
       */
      ignoredProtocols: Array,

      handlersEnabledPref_: {
        type: Object,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

    };
  }

  protocols: ProtocolEntry[];
  appAllowedProtocols: AppProtocolEntry[];
  appDisallowedProtocols: AppProtocolEntry[];
  private showAppsProtocolHandlersTitle_: boolean;
  private actionMenuModel_: HandlerEntry|null;
  toggleOffLabel: string;
  toggleOnLabel: string;
  ignoredProtocols: HandlerEntry[];
  private handlersEnabledPref_: chrome.settingsPrivate.PrefObject<boolean>;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'setHandlersEnabled',
        (enabled: boolean) => this.setHandlersEnabled_(enabled));
    this.addWebUiListener(
        'setProtocolHandlers',
        (protocols: ProtocolEntry[]) => this.setProtocolHandlers_(protocols));
    this.addWebUiListener(
        'setIgnoredProtocolHandlers',
        (ignoredProtocols: HandlerEntry[]) =>
            this.setIgnoredProtocolHandlers_(ignoredProtocols));
    this.browserProxy.observeProtocolHandlers();

    // Web App Observer
    this.addWebUiListener(
        'setAppAllowedProtocolHandlers',
        this.setAppAllowedProtocolHandlers_.bind(this));
    this.addWebUiListener(
        'setAppDisallowedProtocolHandlers',
        this.setAppDisallowedProtocolHandlers_.bind(this));
    this.browserProxy.observeAppProtocolHandlers();
  }

  /**
   * Obtains the description for the main toggle.
   * @return The description to use.
   */
  private computeHandlersDescription_(): string {
    return this.handlersEnabledPref_.value ? this.toggleOnLabel :
                                             this.toggleOffLabel;
  }

  /**
   * Updates the main toggle to set it enabled/disabled.
   * @param enabled The state to set.
   */
  private setHandlersEnabled_(enabled: boolean) {
    this.set('handlersEnabledPref_.value', enabled);
  }

  /**
   * Updates the list of protocol handlers.
   * @param protocols The new protocol handler list.
   */
  private setProtocolHandlers_(protocols: ProtocolEntry[]) {
    this.protocols = protocols;
  }

  /**
   * Updates the list of ignored protocol handlers.
   * @param ignoredProtocols The new (ignored) protocol handler list.
   */
  private setIgnoredProtocolHandlers_(ignoredProtocols: HandlerEntry[]) {
    this.ignoredProtocols = ignoredProtocols;
  }

  /**
   * Updates the list of allowed app protocol handlers.
   * @param appAllowedProtocols The new allowed app protocol handler list.
   */
  private setAppAllowedProtocolHandlers_(appAllowedProtocols:
                                             AppProtocolEntry[]) {
    this.appAllowedProtocols = appAllowedProtocols;
    this.updateShowAppsProtocolHandlersTitle_();
  }

  /**
   * Updates the list of disallowed app protocol handlers.
   * @param appDisallowedProtocols The new disallowed app protocol
   *     handler list.
   */
  private setAppDisallowedProtocolHandlers_(appDisallowedProtocols:
                                                AppProtocolEntry[]) {
    this.appDisallowedProtocols = appDisallowedProtocols;
    this.updateShowAppsProtocolHandlersTitle_();
  }

  /**
   * Determines if the app header should be shown.
   */
  private updateShowAppsProtocolHandlersTitle_() {
    this.showAppsProtocolHandlersTitle_ =
        (this.appAllowedProtocols && this.appAllowedProtocols.length > 0) ||
        (this.appDisallowedProtocols && this.appDisallowedProtocols.length > 0);
  }

  /**
   * Closes action menu and resets action menu model
   */
  private closeActionMenu_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.actionMenuModel_ = null;
  }

  /**
   * A handler when the toggle is flipped.
   */
  private onToggleChange_() {
    this.browserProxy.setProtocolHandlerDefault(
        !!this.handlersEnabledPref_.value);
  }

  /**
   * The handler for when "Set Default" is selected in the action menu.
   */
  private onDefaultClick_() {
    const item = this.actionMenuModel_!;
    this.browserProxy.setProtocolDefault(item.protocol, item.spec);
    this.closeActionMenu_();
  }

  /**
   * The handler for when "Remove" is selected in the action menu.
   */
  private onRemoveClick_() {
    const item = this.actionMenuModel_!;
    this.browserProxy.removeProtocolHandler(item.protocol, item.spec);
    this.closeActionMenu_();
  }

  /**
   * Handler for removing web app protocol handlers that were allowed.
   */
  private onRemoveAppAllowedHandlerButtonClick_(
      event: DomRepeatEvent<AppHandlerEntry>) {
    const item = event.model.item;
    this.browserProxy.removeAppAllowedHandler(
        item.protocol, item.spec, item.app_id);
  }

  /**
   * Handler for removing web app protocol handlers that were disallowed.
   */
  private onRemoveAppDisallowedHandlerButtonClick_(
      event: DomRepeatEvent<AppHandlerEntry>) {
    const item = event.model.item;
    this.browserProxy.removeAppDisallowedHandler(
        item.protocol, item.spec, item.app_id);
  }

  /**
   * Handler for removing handlers that were blocked
   */
  private onRemoveIgnored_(event: DomRepeatEvent<HandlerEntry>) {
    const item = event.model.item;
    this.browserProxy.removeProtocolHandler(item.protocol, item.spec);
  }

  /**
   * A handler to show the action menu next to the clicked menu button.
   */
  private showMenu_(event: DomRepeatEvent<HandlerEntry>) {
    this.actionMenuModel_ = event.model.item;
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(
        event.target as HTMLElement);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'protocol-handlers': ProtocolHandlersElement;
  }
}

customElements.define(ProtocolHandlersElement.is, ProtocolHandlersElement);
