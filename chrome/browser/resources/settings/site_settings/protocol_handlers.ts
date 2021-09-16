// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'protocol-handlers' is the polymer element for showing the
 * protocol handlers category under Site Settings.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../privacy_page/collapse_radio_button.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {SiteSettingsMixin, SiteSettingsMixinInterface} from './site_settings_mixin.js';

/**
 * All possible actions in the menu.
 */
enum MenuActions {
  SET_DEFAULT = 'SetDefault',
  REMOVE = 'Remove',
}

export type HandlerEntry = {
  host: string,
  is_default: boolean,
  protocol: string,
  protocol_display_name: string,
  spec: string,
};

export type ProtocolEntry = {
  handlers: Array<HandlerEntry>,
  protocol: string,
  protocol_display_name: string,
};

interface RepeaterEvent extends Event {
  model: {
    item: HandlerEntry,
  }
}

const ProtocolHandlersElementBase = mixinBehaviors(
                                        [WebUIListenerBehavior],
                                        SiteSettingsMixin(PolymerElement)) as {
  new (): PolymerElement & WebUIListenerBehavior & SiteSettingsMixinInterface
};

class ProtocolHandlersElement extends ProtocolHandlersElementBase {
  static get is() {
    return 'protocol-handlers';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Array of protocols and their handlers.
       */
      protocols: Array,

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

  protocols: Array<ProtocolEntry>;
  private actionMenuModel_: HandlerEntry|null;
  toggleOffLabel: string;
  toggleOnLabel: string;
  ignoredProtocols: Array<HandlerEntry>;
  private handlersEnabledPref_: chrome.settingsPrivate.PrefObject;

  ready() {
    super.ready();

    this.addWebUIListener(
        'setHandlersEnabled',
        (enabled: boolean) => this.setHandlersEnabled_(enabled));
    this.addWebUIListener(
        'setProtocolHandlers',
        (protocols: Array<ProtocolEntry>) =>
            this.setProtocolHandlers_(protocols));
    this.addWebUIListener(
        'setIgnoredProtocolHandlers',
        (ignoredProtocols: Array<HandlerEntry>) =>
            this.setIgnoredProtocolHandlers_(ignoredProtocols));
    this.browserProxy.observeProtocolHandlers();
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
  private setProtocolHandlers_(protocols: Array<ProtocolEntry>) {
    this.protocols = protocols;
  }

  /**
   * Updates the list of ignored protocol handlers.
   * @param ignoredProtocols The new (ignored) protocol handler list.
   */
  private setIgnoredProtocolHandlers_(ignoredProtocols: Array<HandlerEntry>) {
    this.ignoredProtocols = ignoredProtocols;
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
   * Handler for removing handlers that were blocked
   */
  private onRemoveIgnored_(event: RepeaterEvent) {
    const item = event.model.item;
    this.browserProxy.removeProtocolHandler(item.protocol, item.spec);
  }

  /**
   * A handler to show the action menu next to the clicked menu button.
   */
  private showMenu_(event: RepeaterEvent) {
    this.actionMenuModel_ = event.model.item;
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(
        event.target as HTMLElement);
  }
}

customElements.define(ProtocolHandlersElement.is, ProtocolHandlersElement);
