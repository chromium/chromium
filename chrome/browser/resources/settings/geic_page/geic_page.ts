// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../icons.html.js';
import '../settings_page/settings_section.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './geic_page.css.js';
import {getHtml} from './geic_page.html.js';

const SettingsGeicPageElementBase = SettingsViewMixinLit(CrLitElement);

export class SettingsGeicPageElement extends SettingsGeicPageElementBase {
  static get is() {
    return 'settings-geic-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }


  // SettingsViewMixin implementation.
  override getFocusConfig() {
    return new Map([
      [routes.GEMINI_ENTERPRISE.path, '#geicLinkRow'],
    ]);
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    assert(childViewId === 'geminiEnterprise');
    const control = this.shadowRoot.querySelector<HTMLElement>('#geicLinkRow');
    assert(
        control,
        `Failed to find associated control for child '${childViewId}'`);
    return control;
  }

  protected onGeicPageClick_() {
    Router.getInstance().navigateTo(routes.GEMINI_ENTERPRISE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-geic-page': SettingsGeicPageElement;
  }
}

customElements.define(SettingsGeicPageElement.is, SettingsGeicPageElement);
