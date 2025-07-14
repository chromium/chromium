// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://newtab-footer/shared/customize_buttons/customize_buttons.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {NewTabFooterDocumentProxy} from './browser_proxy.js';
import type {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerRemote} from './customize_buttons.mojom-webui.js';
import {CustomizeChromeSection, SidePanelOpenTrigger} from './customize_buttons.mojom-webui.js';
import {CustomizeButtonsProxy} from './customize_buttons_proxy.js';
import type {ManagementNotice, NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerInterface} from './new_tab_footer.mojom-webui.js';
import {WindowProxy} from './window_proxy.js';

// TODO(crbug.com/419144611) Move to a shared util as it's shared by both the
// Ntp and the Footer.
export enum CustomizeDialogPage {
  BACKGROUNDS = 'backgrounds',
  SHORTCUTS = 'shortcuts',
  MODULES = 'modules',
  THEMES = 'themes',
}

/**
 * Customize Chrome entry points. This enum must match the numbering for
 * FooterCustomizeChromeEntryPoint in enums.xml. These values are persisted to
 * logs. Entries should not be renumbered, removed or reused.
 */
export enum FooterCustomizeChromeEntryPoint {
  CUSTOMIZE_BUTTON = 0,
  URL = 1,
  MAX_VALUE = URL,
}

/**
 * Elements on the New Tab Footer. This enum must match the numbering for
 * FooterElement in enums.xml. These values are persisted to logs. Entries
 * should not be renumbered, removed or reused.
 */
export enum FooterElement {
  OTHER = 0,
  CUSTOMIZE_BUTTON = 1,
  EXTENSION_NAME = 2,
  MANAGEMENT_NOTICE = 3,
  BACKGROUND_ATTRIBUTION = 4,
  CONTEXT_MENU = 5,
  MAX_VALUE = CONTEXT_MENU,
}

const CUSTOMIZE_URL_PARAM: string = 'customize';

function recordCustomizeChromeOpen(element: FooterCustomizeChromeEntryPoint) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Footer.CustomizeChromeOpened', element,
      FooterCustomizeChromeEntryPoint.MAX_VALUE + 1);
}

function recordClick(element: FooterElement) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Footer.Click', element, FooterElement.MAX_VALUE + 1);
}

export class NewTabFooterAppElement extends CrLitElement {
  static get is() {
    return 'new-tab-footer-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      extensionName_: {type: String},
      managementNotice_: {type: Object},
      showCustomize_: {type: Boolean},
      showCustomizeChromeText_: {type: Boolean},
    };
  }

  protected accessor extensionName_: string|null = null;
  protected accessor managementNotice_: ManagementNotice|null = null;
  private selectedCustomizeDialogPage_: string|null;
  protected accessor showCustomize_: boolean = false;
  protected accessor showCustomizeChromeText_: boolean = true;

  private callbackRouter_: NewTabFooterDocumentCallbackRouter;
  private handler_: NewTabFooterHandlerInterface;
  private customizeCallbackRouter_: CustomizeButtonsDocumentCallbackRouter;
  private customizeHandler_: CustomizeButtonsHandlerRemote;
  private setCustomizeChromeSidePanelVisibilityListener_: number|null = null;
  private setNtpExtensionNameListenerId_: number|null = null;
  private setManagementNoticeListener_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ =
        NewTabFooterDocumentProxy.getInstance().callbackRouter;
    this.handler_ = NewTabFooterDocumentProxy.getInstance().handler;
    this.customizeCallbackRouter_ =
        CustomizeButtonsProxy.getInstance().callbackRouter;
    this.customizeHandler_ = CustomizeButtonsProxy.getInstance().handler;

    this.showCustomize_ =
        WindowProxy.getInstance().url.searchParams.has(CUSTOMIZE_URL_PARAM);
    this.selectedCustomizeDialogPage_ =
        WindowProxy.getInstance().url.searchParams.get(CUSTOMIZE_URL_PARAM);
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setNtpExtensionNameListenerId_ =
        this.callbackRouter_.setNtpExtensionName.addListener((name: string) => {
          this.extensionName_ = name;
        });
    this.handler_.updateNtpExtensionName();
    this.setManagementNoticeListener_ =
        this.callbackRouter_.setManagementNotice.addListener(
            (notice: ManagementNotice) => {
                this.managementNotice_ = notice;
            });
    this.handler_.updateManagementNotice();
    this.setCustomizeChromeSidePanelVisibilityListener_ =
        this.customizeCallbackRouter_.setCustomizeChromeSidePanelVisibility
            .addListener((visible: boolean) => {
              this.showCustomize_ = visible;
            });
    // Open Customize Chrome if there are Customize Chrome URL params.
    if (this.showCustomize_) {
      this.setCustomizeChromeSidePanelVisible(this.showCustomize_);
      recordCustomizeChromeOpen(FooterCustomizeChromeEntryPoint.URL);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setNtpExtensionNameListenerId_);
    this.callbackRouter_.removeListener(this.setNtpExtensionNameListenerId_);
    assert(this.setManagementNoticeListener_);
    this.callbackRouter_.removeListener(this.setManagementNoticeListener_);
    assert(this.setCustomizeChromeSidePanelVisibilityListener_);
    this.customizeCallbackRouter_.removeListener(
        this.setCustomizeChromeSidePanelVisibilityListener_);
  }

  protected onContextMenu_(e: MouseEvent) {
    this.handler_.showContextMenu({x: e.clientX, y: e.clientY});
    recordClick(FooterElement.CONTEXT_MENU);
  }

  protected onExtensionNameClick_(e: Event) {
    e.preventDefault();
    recordClick(FooterElement.EXTENSION_NAME);
    this.handler_.openExtensionOptionsPageWithFallback();
  }

  protected onCustomizeClick_() {
    recordClick(FooterElement.CUSTOMIZE_BUTTON);
    // Let side panel decide what page or section to show.
    this.selectedCustomizeDialogPage_ = null;
    this.setCustomizeChromeSidePanelVisible(!this.showCustomize_);
    if (!this.showCustomize_) {
      this.customizeHandler_.incrementCustomizeChromeButtonOpenCount();
      recordCustomizeChromeOpen(
          FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON);
    }
  }

  /**
   * Public for testing. Returns the section being shown to allow test
   * verification.
   */
  setCustomizeChromeSidePanelVisible(visible: boolean): CustomizeChromeSection {
    let section: CustomizeChromeSection = CustomizeChromeSection.kUnspecified;
    switch (this.selectedCustomizeDialogPage_) {
      case CustomizeDialogPage.BACKGROUNDS:
      case CustomizeDialogPage.THEMES:
        section = CustomizeChromeSection.kAppearance;
        break;
      case CustomizeDialogPage.SHORTCUTS:
        section = CustomizeChromeSection.kShortcuts;
        break;
      case CustomizeDialogPage.MODULES:
        section = CustomizeChromeSection.kModules;
        break;
    }
    this.customizeHandler_.setCustomizeChromeSidePanelVisible(
        visible, section, SidePanelOpenTrigger.kNewTabFooter);
    return section;
  }

  setSelectedCustomizeDialogPageForTesting(page: CustomizeDialogPage) {
    this.selectedCustomizeDialogPage_ = page;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'new-tab-footer-app': NewTabFooterAppElement;
  }
}

customElements.define(NewTabFooterAppElement.is, NewTabFooterAppElement);
