// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'sanitize-done' is a dialog shown after reverting to safe settings
 * (aka sanitize).
 */
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cros_components/button/button.js';
import './sanitize_shared.css.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sanitize_done.html.js';


const SanitizeDoneElementBase = I18nMixin(PolymerElement);

export class SanitizeDoneElement extends SanitizeDoneElementBase {
  static get is() {
    return 'sanitize-done' as const;
  }

  static get template() {
    return getTemplate();
  }

  private onExtensionsButtonClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://extensions');
  }

  private onChromeOsInputClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl(
        'chrome://os-settings/osLanguages/input');
  }

  private onChromeOsNetworkClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://os-settings/internet');
  }

  private onChromeSiteContentClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://settings/content');
  }

  private onChromeStartupClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://settings/onStartup');
  }

  private onChromeHomepageClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://settings/appearance');
  }

  private onChromeLanguagesClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://settings/languages');
  }

  private onDoneClick_(): void {
    window.close();
  }

  static get properties() {
    return {
      extensionsExpanded_: {
        type: Boolean,
        value: false,
      },

      chromeOSSettingsInfoExpanded_: {
        type: Boolean,
        value: false,
      },

      chromeSettingsInfoExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private extensionsExpanded_: boolean;
  private chromeOSSettingsInfoExpanded_: boolean;
  private chromeSettingsInfoExpanded_: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    [SanitizeDoneElement.is]: SanitizeDoneElement;
  }
}

customElements.define(SanitizeDoneElement.is, SanitizeDoneElement);
ColorChangeUpdater.forDocument().start();
