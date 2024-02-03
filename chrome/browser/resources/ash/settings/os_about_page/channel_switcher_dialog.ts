// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-channel-switcher-dialog' is a component allowing the
 * user to switch between release channels (dev, beta, stable). A
 * |target-channel-changed| event is fired if the user does select a different
 * release channel to notify parents of this dialog.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, BrowserChannel, isTargetChannelMoreStable} from './about_page_browser_proxy.js';
import {getTemplate} from './channel_switcher_dialog.html.js';

const WarningMessage = {
  NONE: -1,
  ENTERPRISE_MANAGED: 0,
  POWERWASH: 1,
  UNSTABLE: 2,
};

export interface SettingsChannelSwitcherDialogElement {
  $: {
    changeChannel: HTMLElement,
    changeChannelAndPowerwash: HTMLElement,
    dialog: CrDialogElement,
    warningSelector: IronSelectorElement,
  };
}

export class SettingsChannelSwitcherDialogElement extends PolymerElement {
  static get is() {
    return 'settings-channel-switcher-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      browserChannelEnum_: {
        type: Object,
        value: BrowserChannel,
      },

      currentChannel_: String,

      targetChannel_: String,

      /**
       * Controls which of the two action buttons is visible.
       */
      shouldShowButtons_: {
        type: Object,
        value: null,
      },
    };
  }

  private browserChannelEnum_: typeof BrowserChannel;
  private currentChannel_: BrowserChannel;
  private targetChannel_: BrowserChannel;
  private shouldShowButtons_: {
    changeChannel: boolean,
    changeChannelAndPowerwash: boolean,
  }|null;

  private browserProxy_: AboutPageBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = AboutPageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.browserProxy_.getChannelInfo().then(info => {
      this.currentChannel_ = info.currentChannel;
      this.targetChannel_ = info.targetChannel;
      // Pre-populate radio group with target channel.
      const radioGroup = this.getRadioGroup_();
      radioGroup.selected = this.targetChannel_;
      radioGroup.focus();
    });
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private getRadioGroup_(): CrRadioGroupElement {
    return castExists(this.shadowRoot!.querySelector('cr-radio-group'));
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onChangeChannelClick_(): void {
    const selectedChannel = this.getRadioGroup_().selected as BrowserChannel;
    this.browserProxy_.setChannel(selectedChannel, false);
    this.$.dialog.close();
    this.fireTargetChannelChangedEvent_(selectedChannel);
  }

  private onChangeChannelAndPowerwashClick_(): void {
    const selectedChannel = this.getRadioGroup_().selected as BrowserChannel;
    this.browserProxy_.setChannel(selectedChannel, true);
    this.$.dialog.close();
    this.fireTargetChannelChangedEvent_(selectedChannel);
  }

  private fireTargetChannelChangedEvent_(detail = {}): void {
    const event = new CustomEvent(
        'target-channel-changed', {bubbles: true, composed: true, detail});
    this.dispatchEvent(event);
  }

  /**
   * @param changeChannel Whether the changeChannel button should be visible.
   * @param changeChannelAndPowerwash Whether the changeChannelAndPowerwash
   *    button should be visible.
   */
  private updateButtons_(
      changeChannel: boolean, changeChannelAndPowerwash: boolean): void {
    if (changeChannel || changeChannelAndPowerwash) {
      // Ensure that at most one button is visible at any given time.
      assert(changeChannel !== changeChannelAndPowerwash);
    }

    this.shouldShowButtons_ = {
      changeChannel: changeChannel,
      changeChannelAndPowerwash: changeChannelAndPowerwash,
    };
  }

  private onChannelSelectionChanged_(): void {
    const selectedChannel = this.getRadioGroup_().selected as BrowserChannel;

    // Selected channel is the same as the target channel so only show 'cancel'.
    if (selectedChannel === this.targetChannel_) {
      this.shouldShowButtons_ = null;
      this.$.warningSelector.select(WarningMessage.NONE);
      return;
    }

    // Selected channel is the same as the current channel, allow the user to
    // change without warnings.
    if (selectedChannel === this.currentChannel_) {
      this.updateButtons_(true, false);
      this.$.warningSelector.select(WarningMessage.NONE);
      return;
    }

    if (isTargetChannelMoreStable(this.currentChannel_, selectedChannel)) {
      // More stable channel selected. For non managed devices, notify the user
      // about powerwash.
      if (loadTimeData.getBoolean('aboutEnterpriseManaged')) {
        this.$.warningSelector.select(WarningMessage.ENTERPRISE_MANAGED);
        this.updateButtons_(true, false);
      } else {
        this.$.warningSelector.select(WarningMessage.POWERWASH);
        this.updateButtons_(false, true);
      }
    } else {
      if (selectedChannel === BrowserChannel.DEV) {
        // Dev channel selected, warn the user.
        this.$.warningSelector.select(WarningMessage.UNSTABLE);
      } else {
        this.$.warningSelector.select(WarningMessage.NONE);
      }
      this.updateButtons_(true, false);
    }
  }

  private substituteString_(format: string, replacement: string): string {
    return loadTimeData.substituteString(format, replacement);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-channel-switcher-dialog': SettingsChannelSwitcherDialogElement;
  }
}

customElements.define(
    SettingsChannelSwitcherDialogElement.is,
    SettingsChannelSwitcherDialogElement);
