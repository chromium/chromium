// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../icons.html.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';
// </if>

import '../settings_page/settings_section.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiPageActions} from '../ai_page/constants.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './glic_page.html.js';

export interface SettingsGlicPageElement {
  $: {
    glicLinkRow: HTMLElement,
    learnMoreLabel: HTMLAnchorElement,
  };
}

const SettingsGlicPageElementBase =
    SettingsViewMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsGlicPageElement extends SettingsGlicPageElementBase {
  static get is() {
    return 'settings-glic-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      spark_: {
        type: String,
        value: () => {
          return loadTimeData.getBoolean('glicAssetsV2Enabled') ?
              'settings-internal:sparkv2' :
              'settings-internal:spark';
        },
      },
    };
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  declare private spark_: string;

  private onGlicPageClick_() {
    Router.getInstance().navigateTo(routes.GEMINI);
  }

  private onSettingsPageLearnMoreClick_(event: Event) {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_COLLAPSED_LEARN_MORE_CLICKED);
    // Prevent navigation to the Glic page if only the learn more link was
    // clicked.
    event.stopPropagation();
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    return new Map([
      [routes.GEMINI.path, '#glicLinkRow'],
    ]);
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    assert(childViewId === 'gemini');
    const control = this.shadowRoot!.querySelector<HTMLElement>('#glicLinkRow');
    assert(control);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-page': SettingsGlicPageElement;
  }
}

customElements.define(SettingsGlicPageElement.is, SettingsGlicPageElement);
