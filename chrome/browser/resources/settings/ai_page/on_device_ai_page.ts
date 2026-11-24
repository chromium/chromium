// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../controls/settings_toggle_button.js';
import '../focus_list_container.js';
import '../icons.html.js';
import '../settings_page/settings_subpage.js';
import '../simple_confirmation_dialog.js';
import './on_device_ai_page_entry.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {OnDeviceAiBrowserProxy, OnDeviceAiCapability, OnDeviceAiEnabled} from './on_device_ai_browser_proxy.js';
import {OnDeviceAiBrowserProxyImpl, OnDeviceModelStatus} from './on_device_ai_browser_proxy.js';
import {getCss} from './on_device_ai_page.css.js';
import {getHtml} from './on_device_ai_page.html.js';

const ON_DEVICE_AI_PREF_KEY = 'synthetic.settings.on_device_ai_enabled';

export interface SettingsOnDeviceAiPageElement {
  $: {
    onDeviceAiToggle: SettingsToggleButtonElement,
    toast: CrToastElement,
  };
}

const SettingsOnDeviceAiPageElementBase =
    WebUiListenerMixinLit(SettingsViewMixinLit(I18nMixinLit(CrLitElement)));

export class SettingsOnDeviceAiPageElement extends
    SettingsOnDeviceAiPageElementBase {
  static get is() {
    return 'settings-on-device-ai-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      capabilities_: {type: Array},
      matchingCapabilities_: {type: Array},
      searchTerm_: {type: String},
      showTurnOffDialog_: {type: Boolean},
      toastLabel_: {type: String},
    };
  }

  protected accessor capabilities_: OnDeviceAiCapability[] = [];
  // The models in `capabilities_` that match `searchTerm_`.
  protected accessor matchingCapabilities_: OnDeviceAiCapability[] = [];
  private accessor searchTerm_: string = '';
  protected accessor showTurnOffDialog_: boolean = false;
  protected accessor toastLabel_: string = '';

  private onDeviceAiBrowserProxy_: OnDeviceAiBrowserProxy =
      OnDeviceAiBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    const setOnDeviceAiPref = (onDeviceAiEnabled: OnDeviceAiEnabled) =>
        this.setOnDeviceAiPref_(onDeviceAiEnabled);
    this.addWebUiListener('on-device-ai-enabled-changed', setOnDeviceAiPref);
    Promise
        .all([
          this.onDeviceAiBrowserProxy_.getOnDeviceAiEnabled(),
          PrefService.getInstance().whenInitialized(),
        ])
        .then(([onDeviceAiEnabled]) => setOnDeviceAiPref(onDeviceAiEnabled));

    this.onDeviceAiBrowserProxy_.getOnDeviceAiCapabilities().then(
        capabilities => this.updateCapabilities_(capabilities));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('capabilities_') ||
        changedPrivateProperties.has('searchTerm_')) {
      this.matchingCapabilities_ = this.computeMatchingCapabilities_();
    }
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }

  protected onSearchTermChanged_(e: CustomEvent<{value: string}>) {
    this.searchTerm_ = e.detail.value;
  }

  protected onOnDeviceAiSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('onDeviceAiLearnMoreUrl'));
  }

  protected onOnDeviceAiSettingsBooleanControlChange_(e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    if (toggle.checked) {
      // Turning on does not need confirmation.
      this.applyToggleValue_(toggle);
      return;
    }

    // Turning off removes downloaded models, so ask the user to confirm first.
    // The toggle stays visually off until the dialog is resolved.
    this.showTurnOffDialog_ = true;
  }

  protected onTurnOffDialogClose_() {
    const dialog =
        this.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assert(dialog);
    if (dialog.wasConfirmed()) {
      this.applyToggleValue_(this.$.onDeviceAiToggle);
    } else {
      this.$.onDeviceAiToggle.resetToPrefValue();
    }

    this.showTurnOffDialog_ = false;
    focusWithoutInk(this.$.onDeviceAiToggle);
  }

  // Commits the toggle's current `checked` state to the synthetic pref and to
  // the browser.
  private applyToggleValue_(toggle: SettingsToggleButtonElement) {
    toggle.sendPrefChange();
    this.onDeviceAiBrowserProxy_.setOnDeviceAiEnabled(toggle.checked);
  }

  protected onEntryDeleteClick_(e: CustomEvent<number>) {
    e.stopPropagation();
    // Look up the model now, since it is no longer in `capabilities_` once the
    // browser has deleted it.
    const capability = this.capabilities_.find(item => item.id === e.detail);
    assert(capability);
    this.onDeviceAiBrowserProxy_.deleteOnDeviceAiCapability(e.detail).then(
        capabilities => {
          this.updateCapabilities_(capabilities);
          this.toastLabel_ = this.i18n(
              'onDeviceAiModelRemovedToastLabel', capability.categoryTitle);
          this.$.toast.show();
        });
  }

  private updateCapabilities_(capabilities: OnDeviceAiCapability[]) {
    this.capabilities_ = capabilities.filter(
        item => item.status === OnDeviceModelStatus.READY ||
            item.status === OnDeviceModelStatus.DOWNLOADING ||
            item.status === OnDeviceModelStatus.UPDATING);
  }

  private computeMatchingCapabilities_(): OnDeviceAiCapability[] {
    const searchTerm = this.searchTerm_.trim().toLowerCase();
    return this.capabilities_.filter(
        item => [item.categoryTitle, item.description, item.modelName].some(
            text => text.toLowerCase().includes(searchTerm)));
  }

  private setOnDeviceAiPref_(onDeviceAiEnabled: OnDeviceAiEnabled) {
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: ON_DEVICE_AI_PREF_KEY,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: onDeviceAiEnabled.enabled,
    };

    if (!onDeviceAiEnabled.allowedByPolicy) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      pref.value = false;
    }

    PrefService.getInstance().setSyntheticPref(pref);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-on-device-ai-page': SettingsOnDeviceAiPageElement;
  }
}

customElements.define(
    SettingsOnDeviceAiPageElement.is, SettingsOnDeviceAiPageElement);
