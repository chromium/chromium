// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import './check_mark_wrapper.js';

import {SpHeading} from 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import {HelpBubbleMixin, HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BackgroundCollection, CollectionImage, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import {getTemplate} from './themes.html.js';
import {WindowProxy} from './window_proxy.js';

export const CHROME_THEME_ELEMENT_ID =
    'CustomizeChromeUI::kChromeThemeElementId';
export const CHROME_THEME_BACK_ELEMENT_ID =
    'CustomizeChromeUI::kChromeThemeBackElementId';

const ThemesElementBase = HelpBubbleMixin(PolymerElement) as
    {new (): PolymerElement & HelpBubbleMixinInterface};

export interface ThemesElement {
  $: {
    refreshDailyToggle: CrToggleElement,
    heading: SpHeading,
  };
}

export class ThemesElement extends ThemesElementBase {
  static get is() {
    return 'customize-chrome-themes';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedCollection: {
        type: Object,
        value: null,
        observer: 'onCollectionChange_',
      },
      header_: String,
      isRefreshToggleChecked_: {
        type: Boolean,
        computed: `computeIsRefreshToggleChecked_(theme_, selectedCollection)`,
      },
      theme_: {
        type: Object,
        value: undefined,
      },
      themes_: Array,
    };
  }

  selectedCollection: BackgroundCollection|null;

  private header_: string;
  private isRefreshToggleChecked_: boolean;
  private theme_: Theme|undefined;
  private themes_: CollectionImage[];

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;
  private previewImageLoadStartEpoch_: number;
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

  override ready() {
    super.ready();
    this.registerHelpBubble(
        CHROME_THEME_BACK_ELEMENT_ID, this.$.heading.getBackButton());
  }

  focusOnBackButton() {
    this.$.heading.getBackButton().focus();
  }

  private onThemesRendered_() {
    const firstTile = this.root!.querySelector('.tile.theme');
    if (firstTile) {
      this.registerHelpBubble(CHROME_THEME_ELEMENT_ID, firstTile);
    }
  }

  private onPreviewImageLoad_() {
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

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }

  private onSelectTheme_(e: DomRepeatEvent<CollectionImage>) {
    const {
      attribution1,
      attribution2,
      attributionUrl,
      imageUrl,
      previewImageUrl,
      collectionId,
    } = e.model.item;
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

  private onRefreshDailyToggleChange_(e: CustomEvent<boolean>) {
    if (e.detail) {
      this.pageHandler_.setDailyRefreshCollectionId(
          this.selectedCollection!.id);
    } else {
      this.pageHandler_.setDailyRefreshCollectionId('');
    }
  }

  private isThemeSelected_(url: string) {
    return this.theme_ && !this.theme_.thirdPartyThemeInfo &&
        this.theme_.backgroundImage &&
        this.theme_.backgroundImage.url.url === url &&
        !this.isRefreshToggleChecked_;
  }

  private getThemeCheckedStatus_(url: string): string {
    return this.isThemeSelected_(url) ? 'true' : 'false';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-themes': ThemesElement;
  }
}

customElements.define(ThemesElement.is, ThemesElement);
