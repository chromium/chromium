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
    launchOnStartupToggle: HTMLInputElement,
    hotkeyToggle: HTMLInputElement,
    ephemeralModelToggle: HTMLInputElement,
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
      launchOnStartupEnabled: {type: Boolean},
      hotkeyEnabled: {type: Boolean},
      ephemeralModelEnabled: {type: Boolean},
      selectedInvocationSource: {type: Number},
      shortcutStatus: {type: String},
    };
  }

  protected accessor bgModeEnabled: boolean = false;
  protected accessor launchOnStartupEnabled: boolean = false;
  protected accessor hotkeyEnabled: boolean = true;
  protected accessor ephemeralModelEnabled: boolean = false;
  protected accessor selectedInvocationSource: InvocationSource =
      InvocationSource.kGlobalHotkey;
  protected accessor shortcutStatus: string = '';

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
        proxy.callbackRouter.onLaunchOnStartupChanged.addListener(
            (enabled: boolean) => {
              this.launchOnStartupEnabled = enabled;
            }));

    this.listenerIds_.push(
        proxy.callbackRouter.onHotkeyChanged.addListener((enabled: boolean) => {
          this.hotkeyEnabled = enabled;
        }));

    this.listenerIds_.push(
        proxy.callbackRouter.onEphemeralModelChanged.addListener(
            (enabled: boolean) => {
              this.ephemeralModelEnabled = enabled;
            }));

    proxy.handler.getBackgroundModeEnabled().then(res => {
      this.bgModeEnabled = res.enabled;
    });

    proxy.handler.getLaunchOnStartupEnabled().then(res => {
      this.launchOnStartupEnabled = res.enabled;
    });

    proxy.handler.getHotkeyEnabled().then(res => {
      this.hotkeyEnabled = res.enabled;
    });

    proxy.handler.getEphemeralModelEnabled().then(res => {
      this.ephemeralModelEnabled = res.enabled;
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

  protected onLaunchOnStartupToggleChange() {
    browserProxyFactory.getInstance().handler.setLaunchOnStartupEnabled(
        this.$.launchOnStartupToggle.checked);
  }

  protected onHotkeyToggleChange() {
    browserProxyFactory.getInstance().handler.setHotkeyEnabled(
        this.$.hotkeyToggle.checked);
  }

  protected onEphemeralModelToggleChange() {
    browserProxyFactory.getInstance().handler.setEphemeralModelEnabled(
        this.$.ephemeralModelToggle.checked);
  }

  protected onInvocationSourceChange() {
    this.selectedInvocationSource =
        Number(this.$.sourceSelect.value) as InvocationSource;
  }

  protected onInvokeClick() {
    browserProxyFactory.getInstance().handler.invokeOmniboxEverywhere(
        this.selectedInvocationSource);
  }

  protected onLaunchWithIphClick() {
    browserProxyFactory.getInstance().handler.invokeOmniboxEverywhere(
        this.selectedInvocationSource);
    browserProxyFactory.getInstance().handler.showLensIph();
  }

  protected async onCreateShortcutClick() {
    this.shortcutStatus = 'Creating Start Menu shortcut...';
    const res = await browserProxyFactory.getInstance()
                    .handler.createStartMenuShortcut();
    this.shortcutStatus = res.success ?
        'Start Menu shortcut created successfully.' :
        'Failed to create Start Menu shortcut.';
  }

  protected async onPinToTaskbarClick() {
    this.shortcutStatus = 'Requesting taskbar pin...';
    const res = await browserProxyFactory.getInstance().handler.pinToTaskbar();
    this.shortcutStatus = res.success ? 'Taskbar pin requested successfully.' :
                                        'Failed to pin to taskbar.';
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-debug-app': OmniboxEverywhereDebugAppElement;
  }
}

customElements.define(
    OmniboxEverywhereDebugAppElement.is, OmniboxEverywhereDebugAppElement);
