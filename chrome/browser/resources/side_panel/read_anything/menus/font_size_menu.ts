// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';
import '../app/icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {VisualBrowserProxy} from '../app/visual_browser_proxy.js';
import {VisualBrowserProxyImpl} from '../app/visual_browser_proxy.js';
import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ToolbarEvent} from '../content/read_anything_types.js';
import {openMenu} from '../shared/common.js';
import {getNewIndex, isArrow} from '../shared/keyboard_util.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import {getCss} from './font_size_menu.css.js';
import {getHtml} from './font_size_menu.html.js';
import type {ToolbarMenu} from './menu_util.js';

export interface FontSizeMenuElement {
  $: {
    lazyMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

// Max number of paragraph elements inside an aria-live region for
// announcing setting changes. Not clearing the element may make
// the announce block too big and waste memory. Trade-off is that every
// MAX_PARAGRAPHS_IN_ANNOUNCE_BLOCK font sizes, there is a chance the
// announcement won't happen the sixth time, if the change is too fast.
// It is unlikely someone will change the font size more than 5 times so
// this covers most use cases.
const MAX_PARAGRAPHS_IN_ANNOUNCE_BLOCK = 5;

const FontSizeMenuElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class FontSizeMenuElement extends FontSizeMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'font-size-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      webuiRoundedIconsEnabled_: {type: Boolean},
    };
  }

  protected accessor webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');

  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private visualBrowserProxy_: VisualBrowserProxy =
      VisualBrowserProxyImpl.getInstance();

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    openMenu(this.$.lazyMenu.get(), anchor, showAtConfig);
  }

  close() {
    this.$.lazyMenu.get().close();
  }

  get(): CrActionMenuElement {
    return this.$.lazyMenu.get();
  }

  protected isFontSizeDefault_(): boolean {
    return this.visualBrowserProxy_.getFontSize() ===
        this.visualBrowserProxy_.getDefaultFontSize();
  }

  protected onDecreaseClick_() {
    this.updateFontSize_(false);
  }

  protected onIncreaseClick_() {
    this.updateFontSize_(true);
  }

  protected onResetClick_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    this.visualBrowserProxy_.onFontSizeReset();
    this.fire(ToolbarEvent.FONT_SIZE);
    this.requestUpdate();
  }

  private updateFontSize_(increase: boolean) {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    const startingSize = this.visualBrowserProxy_.getFontSize();
    this.visualBrowserProxy_.onFontSizeChanged(increase);
    this.fire(ToolbarEvent.FONT_SIZE);
    if (startingSize !== this.visualBrowserProxy_.getFontSize()) {
      this.announceSizeChange_(increase);
    }
    this.requestUpdate();
  }

  private announceSizeChange_(increase: boolean) {
    const text = this.i18n(
        increase ? 'increaseFontSizeAnnouncement' :
                   'decreaseFontSizeAnnouncement');

    const sizeChangeAnnounce =
        this.$.lazyMenu.get().querySelector<HTMLDivElement>('#size-announce');
    if (sizeChangeAnnounce) {
      const paragraph: HTMLParagraphElement = document.createElement('p');
      paragraph.textContent = text;
      sizeChangeAnnounce.appendChild(paragraph);
      if (sizeChangeAnnounce.getElementsByTagName('p').length >
          MAX_PARAGRAPHS_IN_ANNOUNCE_BLOCK) {
        this.restoreAnnounceState_('size-announce');
      }
    }
  }

  private restoreAnnounceState_(id: string) {
    const srNotice = this.$.lazyMenu.get().querySelector<HTMLElement>(`#${id}`);
    if (srNotice) {
      const paragraphs = srNotice.querySelectorAll('p');
      paragraphs.forEach(paragraph => {
        paragraph.remove();
      });
    }
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (!isArrow(e.key)) {
      return;
    }
    e.preventDefault();
    const focusableElements =
        Array.from(this.$.lazyMenu.get().querySelectorAll<HTMLElement>(
            'cr-icon-button, cr-button'));
    assert(e.target instanceof HTMLElement);
    const elementToFocus =
        focusableElements[getNewIndex(e.key, e.target, focusableElements)];
    assert(elementToFocus, 'no element to focus');
    elementToFocus.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'font-size-menu': FontSizeMenuElement;
  }
}

customElements.define(FontSizeMenuElement.is, FontSizeMenuElement);
