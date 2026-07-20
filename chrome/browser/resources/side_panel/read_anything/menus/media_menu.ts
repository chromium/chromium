// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Media menu element used for the improved Read Aloud UI.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {DEFAULT_SETTINGS, SettingsOption, ToolbarEvent} from '../content/read_anything_types.js';
import type {SettingsPrefs, ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {openMenu} from '../shared/common.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getCss} from './action_menu.css.js';
import {getHtml} from './media_menu.html.js';
import {SettingsItemType} from './menu_util.js';
import type {SettingsItem, ToolbarMenu} from './menu_util.js';

export interface MediaMenuElement {
  $: {
    lazyMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

const MediaMenuElementBase = WebUiListenerMixinLit(CrLitElement);

// Stores and propagates the data for the media menu.
export class MediaMenuElement extends MediaMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'media-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      settingsPrefs: {type: Object},
      nonModal: {type: Boolean},
      isSpeechActive: {type: Boolean},
      options_: {type: Array},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;
  accessor isSpeechActive: boolean = false;
  protected accessor options_: SettingsItem[] = [];

  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('settingsPrefs') ||
        changedProperties.has('isSpeechActive')) {
      this.initializeMenuOptions_();
    }
  }

  private initializeMenuOptions_() {
    this.options_ = [
      {
        id: SettingsOption.LINKS,
        icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
            'read-anything:link' :
            'read-anything:links-enabled-old',
        title: loadTimeData.getString('linksLabel'),
        itemType: SettingsItemType.TOGGLE,
        checked: this.settingsPrefs.linksEnabled,
        disabled: this.isSpeechActive,
        ariaLabel: this.getLinkItemLabels_(),
      },
      {
        id: SettingsOption.IMAGES,
        icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
            'read-anything:image' :
            'read-anything:images-enabled-old',
        title: loadTimeData.getString('imagesLabel'),
        itemType: SettingsItemType.TOGGLE,
        checked: this.settingsPrefs.imagesEnabled,
        disabled: this.isSpeechActive,
        ariaLabel: this.getImageItemLabels_(),
      },
    ];
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    openMenu(
        this.$.lazyMenu.get(), anchor, showAtConfig, /* onShow= */ undefined,
        this.nonModal);
  }

  close() {
    this.$.lazyMenu.get().close();
  }

  protected getImageItemLabels_(): string {
    if (chrome.readingMode.imagesEnabled) {
      return loadTimeData.getString('disableImagesLabel');
    }
    return loadTimeData.getString('enableImagesLabel');
  }

  protected getLinkItemLabels_(): string {
    if (chrome.readingMode.linksEnabled) {
      return loadTimeData.getString('disableLinksLabel');
    }
    return loadTimeData.getString('enableLinksLabel');
  }

  protected onToggleItemClick_(e: Event) {
    e.stopImmediatePropagation();
    if (this.isSpeechActive) {
      return;
    }
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number.parseInt(currentTarget.dataset['index']!);
    const item = this.options_[index];
    if (!item || item.disabled) {
      return;
    }

    if (item.id === SettingsOption.LINKS) {
      this.logger_.logTextSettingsChange(
          ReadAnythingSettingsChange.LINKS_ENABLED_CHANGE);
      chrome.readingMode.onLinksEnabledToggled();
      this.fire(ToolbarEvent.LINKS);
      item.ariaLabel = this.getLinkItemLabels_();
      item.checked = chrome.readingMode.linksEnabled;
    } else if (item.id === SettingsOption.IMAGES) {
      this.logger_.logTextSettingsChange(
          ReadAnythingSettingsChange.IMAGES_ENABLED_CHANGE);
      chrome.readingMode.onImagesEnabledToggled();
      this.fire(ToolbarEvent.IMAGES);
      item.ariaLabel = this.getImageItemLabels_();
      item.checked = chrome.readingMode.imagesEnabled;
    }

    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'media-menu': MediaMenuElement;
  }
}

customElements.define(MediaMenuElement.is, MediaMenuElement);
