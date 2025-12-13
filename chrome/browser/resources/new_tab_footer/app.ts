// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://newtab-footer/shared/customize_buttons/customize_buttons.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {NewTabFooterDocumentProxy} from './browser_proxy.js';
import type {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerRemote} from './customize_buttons.mojom-webui.js';
import {SidePanelOpenTrigger} from './customize_buttons.mojom-webui.js';
import {CustomizeButtonsProxy} from './customize_buttons_proxy.js';
import {CustomizeChromeSection} from './customize_chrome.mojom-webui.js';
import type {BackgroundAttribution, ManagementNotice, NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerInterface} from './new_tab_footer.mojom-webui.js';
import {NewTabPageType} from './new_tab_footer.mojom-webui.js';
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

const CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID =
    'CustomizeButtonsHandler::kCustomizeChromeButtonElementId';

function recordCustomizeChromeOpen(element: FooterCustomizeChromeEntryPoint) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Footer.CustomizeChromeOpened', element,
      FooterCustomizeChromeEntryPoint.MAX_VALUE + 1);
}

function recordClick(element: FooterElement) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Footer.Click', element, FooterElement.MAX_VALUE + 1);
}

const NewTabFooterAppElementBase = HelpBubbleMixinLit(CrLitElement);

export class NewTabFooterAppElement extends NewTabFooterAppElementBase {
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
      isCustomizeActive_: {type: Boolean},
      managementNotice_: {type: Object},
      showCustomizeButtons_: {type: Boolean},
      showCustomizeText_: {type: Boolean},
      showExtension_: {type: Boolean},
      ntpType_: {type: Object},
      backgroundAttributionLink_: {type: Object},
      backgroundAttributionText_: {type: String},
      showBackgroundAttribution_: {type: Boolean},
    };
  }

  protected accessor extensionName_: string|null = null;
  protected accessor isCustomizeActive_: boolean = false;
  protected accessor managementNotice_: ManagementNotice|null = null;
  protected accessor showCustomizeButtons_: boolean = false;
  protected accessor showCustomizeText_: boolean = true;
  protected accessor showExtension_: boolean = false;
  protected accessor ntpType_: NewTabPageType = NewTabPageType.kOther;
  protected accessor backgroundAttributionText_: string|null = null;
  protected accessor backgroundAttributionLink_: Url|null = null;
  protected accessor showBackgroundAttribution_: boolean = false;

  private selectedCustomizeDialogPage_: string|null;
  private canCustomizeChrome_: boolean = false;
  private callbackRouter_: NewTabFooterDocumentCallbackRouter;
  private handler_: NewTabFooterHandlerInterface;
  private customizeCallbackRouter_: CustomizeButtonsDocumentCallbackRouter;
  private customizeHandler_: CustomizeButtonsHandlerRemote;
  private setCustomizeChromeSidePanelVisibilityListener_: number|null = null;
  private setNtpExtensionNameListenerId_: number|null = null;
  private setBackgroundAttributionListener_: number|null = null;
  private setManagementNoticeListener_: number|null = null;
  private setAttachedTabStateUpdatedListener_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ =
        NewTabFooterDocumentProxy.getInstance().callbackRouter;
    this.handler_ = NewTabFooterDocumentProxy.getInstance().handler;
    this.customizeCallbackRouter_ =
        CustomizeButtonsProxy.getInstance().callbackRouter;
    this.customizeHandler_ = CustomizeButtonsProxy.getInstance().handler;

    this.isCustomizeActive_ =
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
              this.isCustomizeActive_ = visible;
            });
    this.setAttachedTabStateUpdatedListener_ =
        this.callbackRouter_.attachedTabStateUpdated.addListener(
            (ntpType: NewTabPageType, canCustomizeChrome: boolean) => {
              this.ntpType_ = ntpType;
              this.canCustomizeChrome_ = canCustomizeChrome;
            });
    this.handler_.updateAttachedTabState();
    this.setBackgroundAttributionListener_ =
        this.callbackRouter_.setBackgroundAttribution.addListener(
            (attribution: BackgroundAttribution) => {
              if (attribution) {
                this.backgroundAttributionText_ = attribution.name;
                this.backgroundAttributionLink_ = attribution.url;
              } else {
                this.backgroundAttributionText_ = null;
                this.backgroundAttributionLink_ = null;
              }
            });
    this.handler_.updateBackgroundAttribution();
    // Open Customize Chrome if there are Customize Chrome URL params.
    if (this.isCustomizeActive_) {
      this.setCustomizeChromeSidePanelVisible(this.isCustomizeActive_);
      recordCustomizeChromeOpen(FooterCustomizeChromeEntryPoint.URL);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setNtpExtensionNameListenerId_);
    this.callbackRouter_.removeListener(this.setNtpExtensionNameListenerId_);
    assert(this.setManagementNoticeListener_);
    this.callbackRouter_.removeListener(this.setManagementNoticeListener_);
    assert(this.setAttachedTabStateUpdatedListener_);
    this.callbackRouter_.removeListener(
        this.setAttachedTabStateUpdatedListener_);
    assert(this.setBackgroundAttributionListener_);
    this.callbackRouter_.removeListener(this.setBackgroundAttributionListener_);
    assert(this.setCustomizeChromeSidePanelVisibilityListener_);
    this.customizeCallbackRouter_.removeListener(
        this.setCustomizeChromeSidePanelVisibilityListener_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('ntpType_')) {
      this.showCustomizeButtons_ = this.computeShowCustomizeButtons_();
    }

    if (changedPrivateProperties.has('ntpType_') ||
        changedPrivateProperties.has('extensionName_')) {
      this.showExtension_ = this.computeShowExtension_();
    }

    if (changedPrivateProperties.has('backgroundAttributionText_') ||
        changedPrivateProperties.has('ntpType_')) {
      this.showBackgroundAttribution_ =
          this.computeShowBackgroundAttribution_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('showCustomizeButtons_') &&
        this.showCustomizeButtons_) {
      // Anchoring on the internal icon of the button is required here because
      // the button may collapse (when the main view reaches a minimum width
      // size). The collapse happens by hiding a text, which does not trigger
      // the appropriate notifications for the help bubble to reposition
      // properly. Anchoring to the icon ensures that the help bubble adapts
      // accordingly.
      this.registerHelpBubble(
          CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID,
          ['ntp-customize-buttons', '.customize-icon'], {anchorPaddingTop: 10});
      this.handler_.notifyCustomizationButtonVisible();
    }
  }

  private computeShowCustomizeButtons_(): boolean {
    return this.canCustomizeChrome_ &&
        (this.ntpType_ === NewTabPageType.kFirstPartyWebUI ||
         this.ntpType_ === NewTabPageType.kExtension);
  }

  private computeShowExtension_(): boolean {
    return !!this.extensionName_ && this.ntpType_ === NewTabPageType.kExtension;
  }

  private computeShowBackgroundAttribution_(): boolean {
    return !!this.backgroundAttributionText_ &&
        this.ntpType_ === NewTabPageType.kFirstPartyWebUI;
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

  protected onManagementNoticeClick_(e: Event) {
    e.preventDefault();
    recordClick(FooterElement.MANAGEMENT_NOTICE);
    this.handler_.openManagementPage();
  }

  protected onBackgroundAttributionClick_(e: Event) {
    e.preventDefault();
    recordClick(FooterElement.BACKGROUND_ATTRIBUTION);
    assert(!!this.backgroundAttributionLink_);
    this.handler_.openUrlInCurrentTab(this.backgroundAttributionLink_);
  }

  protected onCustomizeClick_() {
    recordClick(FooterElement.CUSTOMIZE_BUTTON);
    // Let side panel decide what page or section to show.
    this.selectedCustomizeDialogPage_ = null;
    this.setCustomizeChromeSidePanelVisible(!this.isCustomizeActive_);
    if (!this.isCustomizeActive_) {
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
      default:
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
