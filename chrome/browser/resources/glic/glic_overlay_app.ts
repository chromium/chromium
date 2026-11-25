// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';
import './icons.html.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';

// <if expr="not is_android">
import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
// </if>
import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {browserProxyFactory, ErrorPanelType, LoadingStyle} from './glic_overlay.mojom-webui.js';
import type {BrowserProxy, OverlayState} from './glic_overlay.mojom-webui.js';

type ProviderChangePanelType =
    ErrorPanelType.kPendingProviderChangeGeminiToGeic|
    ErrorPanelType.kPendingProviderChangeGeicToGemini|
    ErrorPanelType.kPendingProviderChangeGeicToOtherGeic|
    ErrorPanelType.kProviderSwitchNoticeGeminiToGeic|
    ErrorPanelType.kProviderSwitchNoticeGeicToGemini|
    ErrorPanelType.kProviderSwitchNoticeGeicToOtherGeic;

interface ProviderChangePanel {
  header: string;
  // Each entry is shown as a separate paragraph. glic_overlay.html has three
  // paragraph elements (providerChangeBody1-3), so a body has at most three.
  body: string[];
  // Whether to show the continue button instead of the relaunch button.
  showContinueButton: boolean;
}

const PROVIDER_CHANGE_PANELS:
    Record<ProviderChangePanelType, ProviderChangePanel> = {
      [ErrorPanelType.kPendingProviderChangeGeminiToGeic]: {
        header: 'relaunchRequiredGeminiToGeicNoticeHeader',
        body: ['relaunchRequiredGeminiToGeicNotice'],
        showContinueButton: false,
      },
      [ErrorPanelType.kPendingProviderChangeGeicToGemini]: {
        header: 'relaunchRequiredGeicToGeminiNoticeHeader',
        body: [
          'relaunchRequiredGeicToGeminiNotice1',
          'relaunchRequiredGeicToGeminiNotice2',
          'relaunchRequiredGeicToGeminiNotice3',
        ],
        showContinueButton: false,
      },
      [ErrorPanelType.kPendingProviderChangeGeicToOtherGeic]: {
        header: 'relaunchRequiredGeicToOtherGeicNoticeHeader',
        body: ['relaunchRequiredGeicToOtherGeicNotice'],
        showContinueButton: false,
      },
      [ErrorPanelType.kProviderSwitchNoticeGeminiToGeic]: {
        header: 'providerSwitchNoticeGeminiToGeicHeader',
        body: ['providerSwitchNoticeGeminiToGeic'],
        showContinueButton: true,
      },
      [ErrorPanelType.kProviderSwitchNoticeGeicToGemini]: {
        header: 'providerSwitchNoticeGeicToGeminiHeader',
        body: ['providerSwitchNoticeGeicToGemini'],
        showContinueButton: true,
      },
      [ErrorPanelType.kProviderSwitchNoticeGeicToOtherGeic]: {
        header: 'providerSwitchNoticeGeicToOtherGeicHeader',
        body: ['providerSwitchNoticeGeicToOtherGeic'],
        showContinueButton: true,
      },
    };

interface PageElementTypes {
  panelContainer: HTMLElement;
  loadingPanel: HTMLElement;
  offlinePanel: HTMLElement;
  errorPanel: HTMLElement;
  unavailablePanel: HTMLElement;
  ineligibleAccountPanel: HTMLElement;
  disabledByAdminPanel: HTMLElement;
  signInPanel: HTMLElement;
  geicSignInPanel: HTMLElement;
  locationMismatchPanel: HTMLElement;
  providerChangePanel: HTMLElement;
  providerChangeHeadline: HTMLElement;
  providerChangeBody1: HTMLElement;
  providerChangeBody2: HTMLElement;
  providerChangeBody3: HTMLElement;
  retry: HTMLElement;
  reload: HTMLElement;
  showError: HTMLElement;
  signInButton: HTMLElement;
  geicSignInButton: HTMLElement;
  profilePickerButton: HTMLElement;
  ineligibleAccountHelpButton: HTMLElement;
  locationMismatchHelpButton: HTMLElement;
  disabledByAdminCloseButton: HTMLElement;
  relaunchButton: HTMLElement;
  continueButton: HTMLElement;
}

const $: PageElementTypes = new Proxy({}, {
                              get(_target: object, prop: string) {
                                return getRequiredElement(prop);
                              },
                            }) as unknown as PageElementTypes;

export class GlicOverlayElement extends HTMLElement {
  static get is() {
    return 'glic-overlay';
  }

  private browserProxy: BrowserProxy = browserProxyFactory.getInstance();
  private listenerId: number|null = null;
  private listenersSetUp: boolean = false;

  connectedCallback() {
    if (this.listenerId === null) {
      this.listenerId =
          this.browserProxy.callbackRouter.setOverlayState.addListener(
              this.setOverlayState.bind(this));
    }
    if (!this.listenersSetUp) {
      this.setupListeners();
      this.listenersSetUp = true;
    }
  }

  disconnectedCallback() {
    if (this.listenerId !== null) {
      this.browserProxy.callbackRouter.removeListener(this.listenerId);
      this.listenerId = null;
    }
  }

  private showPanel(activePanel: HTMLElement): void {
    const panels = this.querySelectorAll('#localPanels .dialog.panel');
    for (const panel of panels) {
      (panel as HTMLElement).hidden = panel !== activePanel;
    }
  }

  private setOverlayState(state: OverlayState): void {
    if (state.loading !== undefined) {
      this.showLoading(state.loading);
    } else if (state.error !== undefined) {
      this.showError(state.error);
    } else {
      const panels = this.querySelectorAll('#localPanels .dialog.panel');
      for (const panel of panels) {
        (panel as HTMLElement).hidden = true;
      }
    }
  }

  private showProviderChangePanel(type: ProviderChangePanelType): void {
    const {header, body, showContinueButton} = PROVIDER_CHANGE_PANELS[type];
    $.providerChangeHeadline.textContent = loadTimeData.getString(header);
    const paragraphs = [
      $.providerChangeBody1,
      $.providerChangeBody2,
      $.providerChangeBody3,
    ];
    paragraphs.forEach((paragraph, i) => {
      const key = body[i];
      paragraph.textContent = key ? loadTimeData.getString(key) : '';
      paragraph.hidden = !key;
    });
    $.relaunchButton.hidden = showContinueButton;
    $.continueButton.hidden = !showContinueButton;
    this.showPanel($.providerChangePanel);
    if (!$.providerChangePanel.contains(document.activeElement)) {
      $.providerChangePanel.focus();
    }
  }

  private showSignInPanel(): void {
    if (loadTimeData.getBoolean('usesGeicSignIn')) {
      this.showPanel($.geicSignInPanel);
      if (!$.geicSignInPanel.contains(document.activeElement)) {
        $.geicSignInPanel.focus();
      }
      return;
    }
    this.showPanel($.signInPanel);
  }

  private showError(errorType: ErrorPanelType): void {
    switch (errorType) {
      case ErrorPanelType.kOffline:
        this.showPanel($.offlinePanel);
        break;
      case ErrorPanelType.kError:
        this.showPanel($.errorPanel);
        break;
      case ErrorPanelType.kUnavailable:
        this.showPanel($.unavailablePanel);
        break;
      case ErrorPanelType.kIneligibleAccount:
        this.showPanel($.ineligibleAccountPanel);
        break;
      case ErrorPanelType.kDisabledByAdmin:
        $.disabledByAdminPanel.classList.toggle(
            'show-disabled-by-admin-link', false);
        this.showPanel($.disabledByAdminPanel);
        break;
      case ErrorPanelType.kDisabledByAdminWithLink:
        $.disabledByAdminPanel.classList.toggle(
            'show-disabled-by-admin-link', true);
        this.showPanel($.disabledByAdminPanel);
        break;
      case ErrorPanelType.kSignIn:
        this.showSignInPanel();
        break;
      case ErrorPanelType.kLocationMismatch:
        this.showPanel($.locationMismatchPanel);
        break;
      case ErrorPanelType.kPendingProviderChangeGeminiToGeic:
      case ErrorPanelType.kPendingProviderChangeGeicToGemini:
      case ErrorPanelType.kPendingProviderChangeGeicToOtherGeic:
      case ErrorPanelType.kProviderSwitchNoticeGeminiToGeic:
      case ErrorPanelType.kProviderSwitchNoticeGeicToGemini:
      case ErrorPanelType.kProviderSwitchNoticeGeicToOtherGeic:
        this.showProviderChangePanel(errorType);
        break;
      default: {
        const _exhaustiveCheck: never = errorType;
        console.error(`Unhandled error panel type: ${_exhaustiveCheck}`);
        this.showPanel($.errorPanel);
        break;
      }
    }
  }

  private showLoading(loadingStyle: LoadingStyle): void {
    this.showPanel($.loadingPanel);
    const isSidePanel = loadingStyle === LoadingStyle.kSidePanel;
    document.body.classList.toggle('sidePanel', isSidePanel);
    document.body.classList.toggle('floating', !isSidePanel);
  }

  private setupListeners(): void {
    const closeButtons = this.querySelectorAll('.close-button');
    for (const button of closeButtons) {
      button.addEventListener('click', () => {
        this.browserProxy.handler.onClosePanelClicked();
      });
    }

    $.retry.addEventListener('click', () => {
      this.browserProxy.handler.onRetryClicked();
    });

    $.reload.addEventListener('click', () => {
      this.browserProxy.handler.onRetryClicked();
    });

    if (loadTimeData.getBoolean('showErrorAllowed')) {
      $.showError.hidden = false;
      $.showError.addEventListener('click', () => {
        this.browserProxy.handler.onShowErrorClicked();
      });
    }

    $.signInButton.addEventListener('click', () => {
      this.browserProxy.handler.onSignInClicked();
    });

    $.geicSignInButton.addEventListener('click', () => {
      this.browserProxy.handler.onSignInClicked();
    });

    $.profilePickerButton.addEventListener('click', () => {
      this.browserProxy.handler.onProfilePickerClicked();
    });

    $.ineligibleAccountHelpButton.addEventListener('click', () => {
      this.browserProxy.handler.onIneligibleAccountHelpClicked();
    });

    $.locationMismatchHelpButton.addEventListener('click', () => {
      this.browserProxy.handler.onLocationMismatchHelpClicked();
    });

    $.disabledByAdminCloseButton.addEventListener('click', () => {
      this.browserProxy.handler.onDisabledByAdminCloseClicked();
    });

    $.disabledByAdminPanel.querySelector('a')?.addEventListener('click', () => {
      this.browserProxy.handler.onDisabledByAdminLinkClicked();
    });

    $.relaunchButton.addEventListener('click', () => {
      this.browserProxy.handler.onRelaunchClicked();
    });

    $.continueButton.addEventListener('click', () => {
      this.browserProxy.handler.onProviderSwitchNoticeContinueClicked();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-overlay': GlicOverlayElement;
  }
}

customElements.define(GlicOverlayElement.is, GlicOverlayElement);

if (loadTimeData.valueExists('isAndroidMobile') &&
    loadTimeData.getBoolean('isAndroidMobile')) {
  document.body.classList.add('androidMobile');
}
// <if expr="not is_android">
ColorChangeUpdater.forDocument().start();
// </if>
