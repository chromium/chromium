// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_page/settings_section.js';
import './reset_profile_dialog.js';

import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit, Router} from '../router.js';
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getHtml} from './reset_page.html.js';
import type {SettingsResetProfileDialogElement} from './reset_profile_dialog.js';

export interface SettingsResetPageElement {
  $: {
    resetProfileDialog:
        CrLazyRenderLitElement<SettingsResetProfileDialogElement>,
  };
}

const SettingsResetPageElementBase = RouteObserverMixinLit(CrLitElement);

export class SettingsResetPageElement extends SettingsResetPageElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-reset-page';
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      settingsRefresh2026Enabled_: {type: Boolean},
    };
  }

  protected accessor settingsRefresh2026Enabled_: boolean =
      loadTimeData.getString('settingsRefresh2026') !== '';

  /**
   * RouteObserverMixin
   */
  override currentRouteChanged(route: Route) {
    const lazyRender = this.$.resetProfileDialog;

    if (route === routes.TRIGGERED_RESET_DIALOG ||
        route === routes.RESET_DIALOG) {
      lazyRender.get().show();
    } else {
      const dialog = lazyRender.getIfExists();
      if (dialog) {
        dialog.cancel();
      }
    }
  }

  protected onShowResetProfileDialogClick_() {
    Router.getInstance().navigateTo(
        routes.RESET_DIALOG, new URLSearchParams('origin=userclick'));
  }

  protected onResetProfileDialogClose_() {
    Router.getInstance().navigateTo(routes.RESET_DIALOG.parent!);
    const toFocus = this.shadowRoot.querySelector<HTMLElement>('#resetProfile');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  // SettingsPlugin implementation
  async searchContents(query: string) {
    const searchRequest = await getSearchManager().search(query, this);
    return searchRequest.getSearchResult();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-reset-page': SettingsResetPageElement;
  }
}

customElements.define(SettingsResetPageElement.is, SettingsResetPageElement);
