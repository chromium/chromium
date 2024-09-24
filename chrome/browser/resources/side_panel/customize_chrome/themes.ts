// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import './check_mark_wrapper.js';
import './strings.m.js';

import type {SpHeadingElement} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CustomizeChromeAction, NtpImageType, recordCustomizeChromeAction, recordCustomizeChromeImageError} from './common.js';
import type {BackgroundCollection, CollectionImage, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getCss} from './themes.css.js';
import {getHtml} from './themes.html.js';
import {WindowProxy} from './window_proxy.js';

export const CHROME_THEME_ELEMENT_ID =
    'CustomizeChromeUI::kChromeThemeElementId';
export const CHROME_THEME_BACK_ELEMENT_ID =
    'CustomizeChromeUI::kChromeThemeBackElementId';

const ThemesElementBase = HelpBubbleMixinLit(CrLitElement);

export interface ThemesElement {
  $: {
    refreshDailyToggle: CrToggleElement,
    heading: SpHeadingElement,
  };
}

export class ThemesElement extends ThemesElementBase {
  static get is() {
    return 'customize-chrome-themes';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedCollection: {type: Object},
      header_: {type: String},
      imageErrorDetectionEnabled_: {type: Boolean},
      isRefreshToggleChecked_: {type: Boolean},
      theme_: {type: Object},
      themes_: {type: Array},
    };
  }

  selectedCollection: BackgroundCollection|null = null;

  protected header_: string = '';
  protected imageErrorDetectionEnabled_: boolean =
      loadTimeData.getBoolean('imageErrorDetectionEnabled');
  protected isRefreshToggleChecked_: boolean = false;
  private theme_?: Theme;
  protected themes_: CollectionImage[] = [];

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private previewImageLoadStartEpoch_: number = -1;
  private setThemeListenerId_: number|null = null;

  constructor() {
    super();
    this.pageHandler_ = CustomizeChromeApiProxy.getInstance().handler;
    this.callbackRouter_ = CustomizeChromeApiProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          this.theme_ = theme;
        });
    this.pageHandler_.updateTheme();
    FocusOutlineManager.forDocument(document);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setThemeListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('selectedCollection')) {
      this.onCollectionChange_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('theme_') ||
        changedProperties.has('selectedCollection')) {
      this.isRefreshToggleChecked_ = this.computeIsRefreshToggleChecked_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('themes_') && this.themes_.length > 0) {
      this.onThemesRendered_();
    }
  }

  override firstUpdated() {
    this.registerHelpBubble(
        CHROME_THEME_BACK_ELEMENT_ID, this.$.heading.getBackButton());
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private onThemesRendered_() {
    const firstTile = this.shadowRoot!.querySelector('.tile.theme');
    if (firstTile) {
      this.registerHelpBubble(CHROME_THEME_ELEMENT_ID, firstTile);
    }
  }

  protected shouldShowTheme_(itemLoaded: boolean) {
    return !this.imageErrorDetectionEnabled_ || itemLoaded;
  }

  // TODO(b:367702048) -
  // Record 'NewTabPage.BackgroundService.Images.Headers.ErrorDetected',
  // whenever a preview image fails to load.
  protected onPreviewImageLoad_(e: Event) {
    if (this.imageErrorDetectionEnabled_) {
      const index = Number((e.currentTarget as HTMLElement).dataset['index']);
      assert(this.themes_[index]);
      this.themes_[index].imageVerified = true;
      this.requestUpdate();
    }

    chrome.metricsPrivate.recordValue(
        {
          metricName: 'NewTabPage.Images.ShownTime.ThemePreviewImage',
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
          min: 1,
          max: 60000,  // 60 seconds.
          buckets: 100,
        },
        Math.floor(
            WindowProxy.getInstance().now() -
            this.previewImageLoadStartEpoch_));
  }

  protected onPreviewImageError_() {
    if (this.imageErrorDetectionEnabled_) {
      recordCustomizeChromeImageError(NtpImageType.BACKGROUND_IMAGE);
    }
  }

  private onCollectionChange_() {
    this.header_ = '';
    this.themes_ = [];
    if (this.selectedCollection) {
      this.previewImageLoadStartEpoch_ = WindowProxy.getInstance().now();
      this.pageHandler_.getBackgroundImages(this.selectedCollection!.id)
          .then(({images}) => {
            this.themes_ = images;
          });
      this.header_ = this.selectedCollection.label;
    }
  }

  protected onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  protected onSelectTheme_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const theme = this.themes_[index]!;

    recordCustomizeChromeAction(
        CustomizeChromeAction.FIRST_PARTY_COLLECTION_THEME_SELECTED);
    const {
      attribution1,
      attribution2,
      attributionUrl,
      imageUrl,
      previewImageUrl,
      collectionId,
    } = theme;
    this.pageHandler_.setBackgroundImage(
        attribution1, attribution2, attributionUrl, imageUrl, previewImageUrl,
        collectionId);
  }

  private computeIsRefreshToggleChecked_(): boolean {
    if (!this.selectedCollection) {
      return false;
    }
    return !!this.theme_ && !!this.theme_.backgroundImage &&
        this.theme_.backgroundImage.dailyRefreshEnabled &&
        this.selectedCollection!.id ===
        this.theme_.backgroundImage.collectionId;
  }

  protected onRefreshDailyToggleChange_(e: CustomEvent<boolean>) {
    this.pageHandler_.setDailyRefreshCollectionId(
        e.detail ? this.selectedCollection!.id : '');
  }

  protected isThemeSelected_(url: string): boolean {
    return !!this.theme_ && !this.theme_.thirdPartyThemeInfo &&
        !!this.theme_.backgroundImage &&
        this.theme_?.backgroundImage.url.url === url &&
        !this.isRefreshToggleChecked_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-themes': ThemesElement;
  }
}

customElements.define(ThemesElement.is, ThemesElement);
