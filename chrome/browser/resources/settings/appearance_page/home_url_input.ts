// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `home-url-input` is a single-line text field intending to be used with
 * prefs.homepage
 */
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {CrPolicyPrefMixinLit} from '/shared/settings/controls/cr_policy_pref_mixin_lit.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AppearanceBrowserProxy} from './appearance_browser_proxy.js';
import {AppearanceBrowserProxyImpl} from './appearance_browser_proxy.js';
import {getCss} from './home_url_input.css.js';
import {getHtml} from './home_url_input.html.js';

export interface HomeUrlInputElement {
  $: {
    input: CrInputElement,
  };
}

const HOMEPAGE_PREF_KEY = 'homepage';

const HomeUrlInputElementBase =
    CrPolicyPrefMixinLit(PrefServiceObserverMixinLit(CrLitElement));

export class HomeUrlInputElement extends HomeUrlInputElementBase {
  static get is() {
    return 'home-url-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pref: {type: Object},

      /* Set to true to disable editing the input. */
      disabled: {
        type: Boolean,
        reflect: true,
      },

      canTab: {type: Boolean},

      invalid: {type: Boolean},

      /* The current value of the input, reflected to/from |pref|. */
      value: {type: String},

      label: {type: String},
    };
  }

  override accessor pref: chrome.settingsPrivate.PrefObject<string>|undefined =
      undefined;
  accessor disabled: boolean = false;
  accessor canTab: boolean = false;
  accessor invalid: boolean = false;
  accessor value: string = '';
  accessor label: string = '';
  private browserProxy_: AppearanceBrowserProxy =
      AppearanceBrowserProxyImpl.getInstance();

  constructor() {
    super();

    this.noExtensionIndicator = true;  // Prevent double indicator.
  }

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(HOMEPAGE_PREF_KEY, 'pref');
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('pref')) {
      this.setInputValueFromPref_();
    }
  }

  /**
   * Focuses the 'input' element.
   */
  override focus() {
    this.$.input.focus();
  }

  private setInputValueFromPref_() {
    assert(this.pref);
    assert(this.pref.type === chrome.settingsPrivate.PrefType.URL);
    this.value = this.pref.value;
  }

  /**
   * Gets a tab index for this control if it can be tabbed to.
   */
  protected getTabindex_(): number {
    return this.canTab ? 0 : -1;
  }

  /**
   * Change event handler for cr-input. Updates the pref value.
   * settings-input uses the change event because it is fired by the Enter key.
   */
  protected onChange_() {
    if (this.invalid) {
      this.resetValue_();
      return;
    }

    assert(this.pref!.type === chrome.settingsPrivate.PrefType.URL);
    PrefService.getInstance().setPrefValue(HOMEPAGE_PREF_KEY, this.value);
  }

  private resetValue_() {
    this.invalid = false;
    this.setInputValueFromPref_();
    this.$.input.blur();
  }

  /**
   * Keydown handler to specify enter-key and escape-key interactions.
   */
  protected onKeydown_(event: KeyboardEvent) {
    // If pressed enter when input is invalid, do not trigger on-change.
    if (event.key === 'Enter' && this.invalid) {
      event.preventDefault();
    } else if (event.key === 'Escape') {
      this.resetValue_();
    }

    event.stopPropagation();
  }

  /** @return Whether the element should be disabled. */
  protected isDisabled_() {
    return this.disabled || this.isPrefEnforced();
  }

  protected onInput_() {
    if (this.value === '') {
      this.invalid = false;
      return;
    }

    this.browserProxy_.validateStartupPage(this.value).then(isValid => {
      this.invalid = !isValid;
    });
  }

  protected onKeyup_(e: KeyboardEvent) {
    e.stopPropagation();
  }

  protected onKeypress_(e: KeyboardEvent) {
    e.stopPropagation();
  }

  protected onValueChanged_(e: CustomEvent<{value: string}>) {
    this.value = e.detail.value;
  }

  protected onInvalidChanged_(e: CustomEvent<{value: boolean}>) {
    this.invalid = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'home-url-input': HomeUrlInputElement;
  }
}

customElements.define(HomeUrlInputElement.is, HomeUrlInputElement);
