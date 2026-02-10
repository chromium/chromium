// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app_service_internals.html.js';
import type {AppCapabilityInfo, AppInfo, DebugInfo, PreferredAppInfo, PromiseAppInfo} from './app_service_internals.mojom-webui.js';
import {AppServiceInternalsPageHandler} from './app_service_internals.mojom-webui.js';

export class AppServiceInternalsElement extends CrLitElement {
  static get is() {
    return 'app-service-internals';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      appList_: {type: Array},

      preferredAppList_: {type: Array},

      promiseAppList_: {type: Array},

      appCapabilityList_: {type: Array},
    };
  }

  /** List containing debug information for all installed apps. */
  protected accessor appList_: AppInfo[] = [];
  /** List containing preferred app debug information for installed apps. */
  protected accessor preferredAppList_: PreferredAppInfo[] = [];
  /** List containing debug information for all promise apps. */
  protected accessor promiseAppList_: PromiseAppInfo[] = [];
  /** List containing app capability access information. */
  protected accessor appCapabilityList_: AppCapabilityInfo[] = [];

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'hashchange', () => this.onHashChanged_());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    const remote = AppServiceInternalsPageHandler.getRemote();
    remote.getDebugInfo().then(
        response => this.initializeUi_(response.debugInfo));
  }

  private async initializeUi_(debugInfo: DebugInfo|null) {
    if (debugInfo) {
      this.appList_ = debugInfo.appList;
      this.preferredAppList_ = debugInfo.preferredAppList;
      this.promiseAppList_ = debugInfo.promiseAppList;
      this.appCapabilityList_ = debugInfo.appCapabilityList;
    }
    // Apply any hash only after all the page content has rendered.
    await this.updateComplete;
    this.onHashChanged_();
  }

  /**
   * Manually responds to URL hash changes, since the regular browser handling
   * doesn't work in the Shadow DOM.
   */
  private onHashChanged_() {
    if (!location.hash || !this.shadowRoot) {
      window.scrollTo(0, 0);
      return;
    }

    const selected = this.shadowRoot.querySelector(location.hash);
    if (!selected) {
      return;
    }

    selected.scrollIntoView();
  }

  protected save_() {
    const fileParts: string[] = [];
    fileParts.push('App List\n');
    fileParts.push('========\n\n');
    for (const app of this.appList_) {
      fileParts.push(app.name + '\n');
      fileParts.push('-----\n');
      fileParts.push(app.debugInfo + '\n');
    }

    fileParts.push('Preferred Apps\n');
    fileParts.push('==============\n\n');
    for (const preferredApp of this.preferredAppList_) {
      fileParts.push(preferredApp.name + ' (' + preferredApp.id + ')\n');
      fileParts.push('-----\n');
      fileParts.push(preferredApp.preferredFilters + '\n');
    }

    fileParts.push('App Capabilities\n');
    fileParts.push('================\n\n');
    for (const appCapability of this.appCapabilityList_) {
      fileParts.push(appCapability.name + '\n');
      fileParts.push('-----\n');
      fileParts.push(appCapability.debugInfo + '\n');
    }

    fileParts.push('Promise App List\n');
    fileParts.push('================\n\n');
    for (const promiseApp of this.promiseAppList_) {
      fileParts.push(promiseApp.packageId + '\n');
      fileParts.push('-----\n');
      fileParts.push(promiseApp.debugInfo + '\n');
    }

    const file = new Blob(fileParts);
    const a = document.createElement('a');
    a.href = URL.createObjectURL(file);
    a.download = 'app-service-internals.txt';
    a.click();
    URL.revokeObjectURL(a.href);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-service-internals': AppServiceInternalsElement;
  }
}

customElements.define(
    AppServiceInternalsElement.is, AppServiceInternalsElement);
