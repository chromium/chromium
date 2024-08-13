// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {assert, assertInstanceof} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LanguageBrowserProxyImpl} from './language_browser_proxy.js';
import type {LanguageBrowserProxy} from './language_browser_proxy.js';
import {getTemplate} from './translate_button.html.js';

export interface TranslateButtonElement {
  $: {
    languagePicker: HTMLDivElement,
    sourceAutoDetectButton: CrButtonElement,
    sourceLanguageButton: CrButtonElement,
    sourceLanguagePickerContainer: DomRepeat,
    sourceLanguagePickerMenu: HTMLDivElement,
    targetLanguageButton: CrButtonElement,
    targetLanguagePickerContainer: DomRepeat,
    targetLanguagePickerMenu: HTMLDivElement,
    translateButton: CrButtonElement,
  };
}

export class TranslateButtonElement extends PolymerElement {
  static get is() {
    return 'translate-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isTranslateModeEnabled: {
        type: Boolean,
        reflectToAttribute: true,
      },
      shouldShowStarsIcon: {
        type: Boolean,
        computed: 'computeShouldShowStarsIcon(sourceLanguageDisplayName)',
        reflectToAttribute: true,
      },
      sourceLanguageDisplayName: String,
      sourceLanguageMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
      targetLanguageDisplayName: String,
      targetLanguageMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  // Whether the translate mode on the lens overlay has been enabled.
  private isTranslateModeEnabled: boolean = false;
  // Whether the stars icon is visible on the source language button.
  private shouldShowStarsIcon: boolean;
  // The display name of the source translate language.
  private sourceLanguageDisplayName: string =
      loadTimeData.getString('autoDetect');
  // Whether the source language menu picker is visible.
  private sourceLanguageMenuVisible: boolean = false;
  // Whether the target language menu picker is visible.
  private targetLanguageMenuVisible: boolean = false;
  // The display name of the target translate language.
  private targetLanguageDisplayName: string = '';
  // The list of target languages provided by the chrome API.
  private translateLanguageList: chrome.languageSettingsPrivate.Language[];
  private languageBrowserProxy: LanguageBrowserProxy =
      LanguageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.languageBrowserProxy.getLanguageList().then(
        this.onLanguageListRetrieved.bind(this));
  }

  private onLanguageListRetrieved(
      languageList: chrome.languageSettingsPrivate.Language[]) {
    this.translateLanguageList = languageList;

    // After receiving the language list, get the default translate target
    // language. This needs to happen after fetching the language list so we can
    //  use the list to fetch the language's display name.
    this.languageBrowserProxy.getTranslateTargetLanguage().then(
        this.onTargetLanguageRetrieved.bind(this));
  }

  private onTargetLanguageRetrieved(languageCode: string) {
    const defaultLanguage = this.translateLanguageList.find(
        language => language.code === languageCode);
    assert(defaultLanguage);
    this.targetLanguageDisplayName = defaultLanguage.displayName;
  }

  private onAutoDetectMenuItemClick() {
    this.sourceLanguageDisplayName = loadTimeData.getString('autoDetect');
    this.hideLanguagePickerMenus();
  }

  private onSourceLanguageButtonClick() {
    this.sourceLanguageMenuVisible = !this.sourceLanguageMenuVisible;
    this.targetLanguageMenuVisible = false;
  }

  private onTargetLanguageButtonClick() {
    this.targetLanguageMenuVisible = !this.targetLanguageMenuVisible;
    this.sourceLanguageMenuVisible = false;
  }

  private onSourceLanguageMenuItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newSourceLanguage =
        this.$.sourceLanguagePickerContainer.itemForElement(event.target);
    this.sourceLanguageDisplayName = newSourceLanguage.displayName;
    this.hideLanguagePickerMenus();
  }

  private onTargetLanguageMenuItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newTargetLanguage =
        this.$.targetLanguagePickerContainer.itemForElement(event.target);
    this.targetLanguageDisplayName = newTargetLanguage.displayName;
    this.hideLanguagePickerMenus();
  }

  private onTranslateButtonClick() {
    // Toggle translate mode on button click.
    this.isTranslateModeEnabled = !this.isTranslateModeEnabled;
  }

  private hideLanguagePickerMenus() {
    this.targetLanguageMenuVisible = false;
    this.sourceLanguageMenuVisible = false;
  }

  private computeShouldShowStarsIcon(): boolean {
    return this.sourceLanguageDisplayName ===
        loadTimeData.getString('autoDetect');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'translate-button': TranslateButtonElement;
  }
}

customElements.define(TranslateButtonElement.is, TranslateButtonElement);
