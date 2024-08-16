// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './theme_snapshot.js';
import './hover_button.js';
import './strings.m.js'; // Required by <managed-dialog>.
import 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrA11yAnnouncerElement} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './appearance.css.js';
import {getHtml} from './appearance.html.js';
import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import type {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerInterface, Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface AppearanceElement {
  $: {
    chromeColors: HTMLElement,
    editThemeButton: HTMLButtonElement,
    themeSnapshot: HTMLElement,
    setClassicChromeButton: HTMLButtonElement,
    thirdPartyThemeLinkButton: HTMLButtonElement,
    followThemeToggle: HTMLElement,
    followThemeToggleControl: CrToggleElement,
    uploadedImageButton: HTMLButtonElement,
    searchedImageButton: HTMLButtonElement,
  };
}

const AppearanceElementBase = I18nMixinLit(CrLitElement);

export class AppearanceElement extends AppearanceElementBase {
  static get is() {
    return 'customize-chrome-appearance';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme_: {type: Object},
      editThemeButtonText_: {type: String},

      thirdPartyThemeId_: {
        type: String,
        reflect: true,
      },

      thirdPartyThemeName_: {
        type: String,
        reflect: true,
      },

      showBottomDivider_: {type: Boolean},
      showClassicChromeButton_: {type: Boolean},
      showColorPicker_: {type: Boolean},
      showDeviceThemeToggle_: {type: Boolean},
      showThemeSnapshot_: {type: Boolean},
      showUploadedImageButton_: {type: Boolean},
      showSearchedImageButton_: {type: Boolean},
      showManagedDialog_: {type: Boolean},
      isSourceTabFirstPartyNtp_: {type: Boolean},

      wallpaperSearchButtonEnabled_: {
        type: Boolean,
        reflect: true,
      },

      wallpaperSearchEnabled_: {type: Boolean},
    };
  }

  protected theme_?: Theme;
  protected editThemeButtonText_: string = '';
  protected thirdPartyThemeId_: string|null = null;
  protected thirdPartyThemeName_: string|null = null;
  protected showBottomDivider_: boolean = false;
  protected showClassicChromeButton_: boolean = false;
  protected showColorPicker_: boolean = false;
  protected showDeviceThemeToggle_: boolean = false;
  protected showThemeSnapshot_: boolean = false;
  protected showUploadedImageButton_: boolean = false;
  protected showSearchedImageButton_: boolean = false;
  protected showManagedDialog_: boolean = false;
  protected wallpaperSearchButtonEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchButtonEnabled');
  private wallpaperSearchEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchEnabled');
  protected isSourceTabFirstPartyNtp_: boolean = true;
  protected ntpManagedByName_: string = '';
  private setThemeListenerId_: number|null = null;
  private attachedTabStateUpdatedId_: number|null = null;
  private ntpManagedByNameUpdatedId_: number|null = null;

  private callbackRouter_: CustomizeChromePageCallbackRouter;
  private pageHandler_: CustomizeChromePageHandlerInterface;


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

    this.attachedTabStateUpdatedId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.attachedTabStateUpdated.addListener(
                (isSourceTabFirstPartyNtp: boolean) => {
                  this.isSourceTabFirstPartyNtp_ = isSourceTabFirstPartyNtp;
                });
    this.pageHandler_.updateAttachedTabState();

    this.ntpManagedByNameUpdatedId_ =
        CustomizeChromeApiProxy.getInstance()
            .callbackRouter.ntpManagedByNameUpdated.addListener(
                (ntpManagedByName: string) => {
                  this.ntpManagedByName_ = ntpManagedByName;
                });
    this.pageHandler_.updateNtpManagedByName();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setThemeListenerId_);
    this.callbackRouter_.removeListener(this.setThemeListenerId_);

    assert(this.attachedTabStateUpdatedId_);
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.attachedTabStateUpdatedId_);

    assert(this.ntpManagedByNameUpdatedId_);
    CustomizeChromeApiProxy.getInstance().callbackRouter.removeListener(
        this.ntpManagedByNameUpdatedId_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    this.editThemeButtonText_ = this.computeEditThemeButtonText_();

    if (changedPrivateProperties.has('theme_') ||
        changedPrivateProperties.has('isSourceTabFirstPartyNtp_')) {
      this.thirdPartyThemeId_ = this.computeThirdPartyThemeId_();
      this.thirdPartyThemeName_ = this.computeThirdPartyThemeName_();
      this.showClassicChromeButton_ = this.computeShowClassicChromeButton_();
      this.showColorPicker_ = this.computeShowColorPicker_();
      this.showDeviceThemeToggle_ = this.computeShowDeviceThemeToggle_();
      this.showThemeSnapshot_ = this.computeShowThemeSnapshot_();
      this.showUploadedImageButton_ = this.computeShowUploadedImageButton_();
      this.showSearchedImageButton_ = this.computeShowSearchedImageButton_();
    }

    this.showBottomDivider_ = this.computeShowBottomDivider_();

    // Announce when theme is set to Classic Chrome.
    // This should only be triggered if the classic chrome's button is hidden
    // after the initial theme value has already been set.
    if (changedPrivateProperties.has('theme_') &&
        changedPrivateProperties.has('showClassicChromeButton_') &&
        !!changedPrivateProperties.get('theme_') &&
        !this.showClassicChromeButton_) {
      const announcer = getAnnouncerInstance() as CrA11yAnnouncerElement;
      announcer.announce(this.i18n('updatedToClassicChrome'));
      // If the classicChrome button has focus, change focus to editTheme
      // button, since the button is disappearing.
      if (this.shadowRoot!.activeElement === this.$.setClassicChromeButton) {
        this.focusOnThemeButton();
      }
    }
  }

  focusOnThemeButton() {
    this.$.editThemeButton.focus();
  }

  private computeEditThemeButtonText_(): string {
    return this.i18n(
        this.wallpaperSearchButtonEnabled_ ? 'categoriesHeader' :
                                             'changeTheme');
  }

  private computeThirdPartyThemeId_(): string|null {
    if (this.theme_ && this.theme_.thirdPartyThemeInfo) {
      return this.theme_.thirdPartyThemeInfo.id;
    } else {
      return null;
    }
  }

  private computeThirdPartyThemeName_(): string|null {
    if (this.theme_ && this.theme_.thirdPartyThemeInfo) {
      return this.theme_.thirdPartyThemeInfo.name;
    } else {
      return null;
    }
  }

  private computeShowBottomDivider_(): boolean {
    return !!(this.showClassicChromeButton_ || this.showDeviceThemeToggle_);
  }

  private computeShowClassicChromeButton_(): boolean {
    return !!(
        this.theme_ &&
        (this.theme_.backgroundImage || this.theme_.thirdPartyThemeInfo));
  }

  private computeShowColorPicker_(): boolean {
    return !!this.theme_ && !this.theme_.thirdPartyThemeInfo;
  }

  private computeShowDeviceThemeToggle_(): boolean {
    return loadTimeData.getBoolean('showDeviceThemeToggle') &&
        !(!!this.theme_ && !!this.theme_.thirdPartyThemeInfo);
  }

  private computeShowThemeSnapshot_(): boolean {
    return !!this.theme_ && !this.theme_.thirdPartyThemeInfo &&
        (!(this.theme_.backgroundImage &&
           this.theme_.backgroundImage.isUploadedImage)) &&
        this.isSourceTabFirstPartyNtp_;
  }

  private computeShowUploadedImageButton_(): boolean {
    return !!(
        this.theme_ && this.theme_.backgroundImage &&
        this.theme_.backgroundImage.isUploadedImage &&
        !this.theme_.backgroundImage.localBackgroundId);
  }

  private computeShowSearchedImageButton_(): boolean {
    return !!(
        this.theme_ && this.theme_.backgroundImage &&
        this.theme_.backgroundImage.localBackgroundId);
  }

  protected onEditThemeClicked_() {
    recordCustomizeChromeAction(CustomizeChromeAction.EDIT_THEME_CLICKED);
    if (this.handleClickForManagedThemes_()) {
      return;
    }
    this.dispatchEvent(new Event('edit-theme-click'));
  }

  protected onWallpaperSearchClicked_() {
    recordCustomizeChromeAction(
        CustomizeChromeAction.WALLPAPER_SEARCH_APPEARANCE_BUTTON_CLICKED);
    if (this.handleClickForManagedThemes_()) {
      return;
    }
    this.dispatchEvent(new Event('wallpaper-search-click'));
  }

  protected onThirdPartyThemeLinkButtonClick_() {
    if (this.thirdPartyThemeId_) {
      this.pageHandler_.openThirdPartyThemePage(this.thirdPartyThemeId_);
    }
  }

  protected onUploadedImageButtonClick_() {
    this.pageHandler_.chooseLocalCustomBackground();
  }

  protected onSearchedImageButtonClick_() {
    if (this.wallpaperSearchEnabled_) {
      this.dispatchEvent(new CustomEvent('wallpaper-search-click'));
    } else {
      this.dispatchEvent(new Event('edit-theme-click'));
    }
  }

  protected onSetClassicChromeClicked_() {
    if (this.handleClickForManagedThemes_()) {
      return;
    }
    this.pageHandler_.removeBackgroundImage();
    this.pageHandler_.setDefaultColor();
    recordCustomizeChromeAction(
        CustomizeChromeAction.SET_CLASSIC_CHROME_THEME_CLICKED);
  }

  protected onFollowThemeToggleChange_(e: CustomEvent<boolean>) {
    this.pageHandler_.setFollowDeviceTheme(e.detail);
  }

  protected onManagedDialogClosed_() {
    this.showManagedDialog_ = false;
  }

  protected onNewTabPageManageByButtonClicked_() {
    this.pageHandler_.openNtpManagedByPage();
  }

  private handleClickForManagedThemes_(): boolean {
    if (!this.theme_ || !this.theme_.backgroundManagedByPolicy) {
      return false;
    }
    this.showManagedDialog_ = true;
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-appearance': AppearanceElement;
  }
}

customElements.define(AppearanceElement.is, AppearanceElement);
