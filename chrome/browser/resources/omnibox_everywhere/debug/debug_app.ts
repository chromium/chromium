// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, InvocationSource} from '../omnibox_everywhere_debug.mojom-webui.js';

import {getCss} from './debug_app.css.js';
import {getHtml} from './debug_app.html.js';

export interface InvocationSourceOption {
  name: string;
  value: InvocationSource;
}

export interface OmniboxEverywhereDebugAppElement {
  $: {
    bgModeToggle: HTMLInputElement,
    hotkeyToggle: HTMLInputElement,
    sourceSelect: HTMLSelectElement,
  };
}

export class OmniboxEverywhereDebugAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-everywhere-debug-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      bgModeEnabled: {type: Boolean},
      hotkeyEnabled: {type: Boolean},
      selectedInvocationSource: {type: Number},
    };
  }

  protected accessor bgModeEnabled: boolean = false;
  protected accessor hotkeyEnabled: boolean = true;
  protected accessor selectedInvocationSource: InvocationSource =
      InvocationSource.kGlobalHotkey;

  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    const proxy = browserProxyFactory.getInstance();

    this.listenerIds_.push(
        proxy.callbackRouter.onBackgroundModeChanged.addListener(
            (enabled: boolean) => {
              this.bgModeEnabled = enabled;
            }));

    this.listenerIds_.push(
        proxy.callbackRouter.onHotkeyChanged.addListener((enabled: boolean) => {
          this.hotkeyEnabled = enabled;
        }));

    proxy.handler.getBackgroundModeEnabled().then(res => {
      this.bgModeEnabled = res.enabled;
    });

    proxy.handler.getHotkeyEnabled().then(res => {
      this.hotkeyEnabled = res.enabled;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const router = browserProxyFactory.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => router.removeListener(id));
    this.listenerIds_ = [];
  }

  protected computeInvocationSourceOptions(): InvocationSourceOption[] {
    return Object.keys(InvocationSource)
        .filter(
            key => isNaN(Number(key)) && key !== 'MIN_VALUE' &&
                key !== 'MAX_VALUE')
        .map(key => ({
               name: key,
               value: InvocationSource[key as keyof typeof InvocationSource],
             }));
  }

  protected isSource(source: InvocationSource): boolean {
    return this.selectedInvocationSource === source;
  }

  protected onBgModeToggleChange() {
    browserProxyFactory.getInstance().handler.setBackgroundModeEnabled(
        this.$.bgModeToggle.checked);
  }

  protected onHotkeyToggleChange() {
    browserProxyFactory.getInstance().handler.setHotkeyEnabled(
        this.$.hotkeyToggle.checked);
  }

  protected onInvocationSourceChange() {
    this.selectedInvocationSource =
        Number(this.$.sourceSelect.value) as InvocationSource;
  }

  protected onInvokeClick() {
    browserProxyFactory.getInstance().handler.invokeOmniboxEverywhere(
        this.selectedInvocationSource);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-debug-app': OmniboxEverywhereDebugAppElement;
  }
}

customElements.define(
    OmniboxEverywhereDebugAppElement.is, OmniboxEverywhereDebugAppElement);
