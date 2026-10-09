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

import {SettingsOption, ToolbarEvent} from '../content/read_anything_types.js';
import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {openMenu} from '../shared/common.js';

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
      linksEnabled: {type: Boolean},
      imagesEnabled: {type: Boolean},
      nonModal: {type: Boolean},
      isSpeechActive: {type: Boolean},
      options_: {type: Array},
    };
  }

  accessor linksEnabled: boolean = false;
  accessor imagesEnabled: boolean = false;
  accessor nonModal: boolean = false;
  accessor isSpeechActive: boolean = false;
  protected accessor options_: SettingsItem[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('linksEnabled') ||
        changedProperties.has('imagesEnabled') ||
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
        checked: this.linksEnabled,
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
        checked: this.imagesEnabled,
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
    return loadTimeData.getString(
        this.imagesEnabled ? 'disableImagesLabel' : 'enableImagesLabel');
  }

  protected getLinkItemLabels_(): string {
    return loadTimeData.getString(
        this.linksEnabled ? 'disableLinksLabel' : 'enableLinksLabel');
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
      this.fire(ToolbarEvent.LINKS);
    } else if (item.id === SettingsOption.IMAGES) {
      this.fire(ToolbarEvent.IMAGES);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'media-menu': MediaMenuElement;
  }
}

customElements.define(MediaMenuElement.is, MediaMenuElement);
