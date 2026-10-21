// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Must be imported first so that `loadTimeData` is populated before any
// element below reads from it.
import '/strings.m.js';
import '/ntp/logo.js';
import '/ntp/ntp_searchbox.js';

import type {Theme} from '/ntp/new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from '/ntp/new_tab_page_proxy.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

const NtpExtensionAppElementBase = I18nMixinLit(CrLitElement);

// A minimal New Tab Page hosted in a component extension. Reuses the
// <ntp-logo> (Google logo / doodle) and <ntp-searchbox> (realbox) elements
// from chrome://new-tab-page.
export class NtpExtensionAppElement extends NtpExtensionAppElementBase {
  static get is() {
    return 'ntp-extension-app';
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
    };
  }

  protected accessor theme_: Theme|null = null;

  private setThemeListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
    this.setThemeListenerId_ =
        NewTabPageProxy.getInstance().callbackRouter.setTheme.addListener(
            (theme: Theme) => {
              this.theme_ = theme;
            });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.setThemeListenerId_ !== null) {
      NewTabPageProxy.getInstance().callbackRouter.removeListener(
          this.setThemeListenerId_);
      this.setThemeListenerId_ = null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('theme_') && this.theme_) {
      document.body.style.backgroundColor =
          skColorToRgba(this.theme_.backgroundColor);
      const logoColor = this.getLogoColor_();
      if (logoColor) {
        this.style.setProperty('--ntp-logo-color', skColorToRgba(logoColor));
      } else {
        this.style.removeProperty('--ntp-logo-color');
      }
    }
  }

  // As on chrome://new-tab-page, the logo is single-colored, white by default,
  // in dark mode.
  protected isLogoSingleColored_(): boolean {
    return !!this.theme_ && (!!this.theme_.logoColor || this.theme_.isDark);
  }

  private getLogoColor_(): SkColor|null {
    if (!this.theme_) {
      return null;
    }
    return this.theme_.logoColor ||
        (this.theme_.isDark ? hexColorToSkColor('#ffffff') : null);
  }

  protected isThemeDark_(): boolean {
    return !!this.theme_?.isDark;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-extension-app': NtpExtensionAppElement;
  }
}

customElements.define(NtpExtensionAppElement.is, NtpExtensionAppElement);
