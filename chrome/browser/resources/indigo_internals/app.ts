// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {assertNotReachedCase} from 'chrome://resources/js/assert.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './indigo_internals.mojom-webui.js';
import type {CombinedEligibility, GlicPromptInfo, IndigoPrompt} from './indigo_internals.mojom-webui.js';
import {LocalEligibility, OptimizationGuideStatus} from './indigo_internals.mojom-webui.js';

export class IndigoInternalsAppElement extends CrLitElement {
  static get is() {
    return 'indigo-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      localEligibility_: {type: Number},
      optimizationGuideStatus_: {type: Number},
      combinedEligibility_: {type: Object},
      lastUpdated_: {type: String},
      loadedPrompts_: {type: Array},
      currentPromptKey_: {type: String},
      overridePrompt_: {type: String},
      integrationEnabled_: {type: Boolean},
    };
  }

  protected accessor localEligibility_: LocalEligibility|null = null;
  protected accessor optimizationGuideStatus_: OptimizationGuideStatus|null =
      null;
  protected accessor combinedEligibility_: CombinedEligibility|null = null;
  protected accessor lastUpdated_: string = '';
  protected accessor loadedPrompts_: IndigoPrompt[] = [];
  protected accessor currentPromptKey_: string = '';
  protected accessor overridePrompt_: string = '';
  protected accessor integrationEnabled_: boolean = false;
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    const proxy = browserProxyFactory.getInstance();

    proxy.handler.getLocalEligibility().then(
        ({status}: {status: LocalEligibility}) => {
          this.localEligibility_ = status;
        });
    proxy.handler.getOptimizationGuideStatus().then(
        ({status}: {status: OptimizationGuideStatus}) => {
          this.optimizationGuideStatus_ = status;
        });
    proxy.handler.getGlicPromptInfo().then(({info}: {info: GlicPromptInfo}) => {
      this.integrationEnabled_ = info.integrationEnabled;
      this.currentPromptKey_ = info.currentKey;
      this.overridePrompt_ = info.overridePrompt;
      this.loadedPrompts_ = info.loadedPrompts;
    });

    this.listenerIds_ = [
      proxy.callbackRouter.onLocalEligibilityChanged.addListener(
          (status: LocalEligibility) => {
            this.localEligibility_ = status;
          }),
      proxy.callbackRouter.onOptimizationGuideStatusChanged.addListener(
          (status: OptimizationGuideStatus) => {
            this.optimizationGuideStatus_ = status;
          }),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => browserProxyFactory.getInstance().callbackRouter.removeListener(
            id));
    this.listenerIds_ = [];
  }

  protected getValueClass_(value: boolean|undefined): string {
    if (value === undefined) {
      return 'value-na';
    }
    return value ? 'value-true' : 'value-false';
  }

  protected getValueText_(value: boolean|undefined): string {
    if (value === undefined) {
      return 'N/A';
    }
    return value.toString();
  }

  protected getLocalEligibilityText_(): string {
    switch (this.localEligibility_) {
      case null:
        return 'Loading...';
      case LocalEligibility.kEligible:
        return 'Eligible';
      case LocalEligibility.kNotSignedIn:
        return 'Not Signed In';
      case LocalEligibility.kMissingCapabilities:
        return 'Missing Capabilities';
      case LocalEligibility.kDisabledByPolicy:
        return 'Disabled By Policy';
      case LocalEligibility.kMissingScript:
        return 'Missing Script';
      case LocalEligibility.kRefreshTokenInPersistentErrorState:
        return 'Refresh Token In Persistent Error State';
      case LocalEligibility.kManagedDomain:
        return 'Managed Domain';
      case LocalEligibility.kGlicDisabledForProfile:
        return 'Glic Disabled For Profile';
      default:
        assertNotReachedCase(this.localEligibility_);
    }
  }

  protected getLocalEligibilityClass_(): string {
    switch (this.localEligibility_) {
      case null:
        return 'status-loading';
      case LocalEligibility.kEligible:
        return 'status-eligible';
      case LocalEligibility.kNotSignedIn:
      case LocalEligibility.kRefreshTokenInPersistentErrorState:
      case LocalEligibility.kMissingCapabilities:
      case LocalEligibility.kDisabledByPolicy:
      case LocalEligibility.kMissingScript:
      case LocalEligibility.kManagedDomain:
      case LocalEligibility.kGlicDisabledForProfile:
        return 'status-ineligible';
      default:
        assertNotReachedCase(this.localEligibility_);
    }
  }

  protected getOptimizationGuideStatusText_(): string {
    switch (this.optimizationGuideStatus_) {
      case null:
        return 'Loading...';
      case OptimizationGuideStatus.kDisabled:
        return 'Disabled';
      case OptimizationGuideStatus.kNotPermitted:
        return 'Not Permitted';
      case OptimizationGuideStatus.kEnabled:
        return 'Enabled';
      default:
        assertNotReachedCase(this.optimizationGuideStatus_);
    }
  }

  protected getOptimizationGuideStatusClass_(): string {
    switch (this.optimizationGuideStatus_) {
      case null:
        return 'status-loading';
      case OptimizationGuideStatus.kDisabled:
      case OptimizationGuideStatus.kNotPermitted:
        return 'status-ineligible';
      case OptimizationGuideStatus.kEnabled:
        return 'status-eligible';
      default:
        assertNotReachedCase(this.optimizationGuideStatus_);
    }
  }

  protected getIntegrationStatusText_(): string {
    return this.integrationEnabled_ ? 'Enabled' : 'Disabled';
  }

  protected getIntegrationStatusClass_(): string {
    return this.integrationEnabled_ ? 'status-eligible' : 'status-ineligible';
  }

  protected async onFetchCombinedClick_() {
    const {status} = await browserProxyFactory.getInstance()
                         .handler.getCombinedEligibility();
    this.combinedEligibility_ = status;
    this.lastUpdated_ = new Date().toLocaleTimeString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-internals-app': IndigoInternalsAppElement;
  }
}

customElements.define(IndigoInternalsAppElement.is, IndigoInternalsAppElement);
