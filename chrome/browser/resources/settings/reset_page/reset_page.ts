// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 */
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import './reset_profile_dialog.js';

import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import {getTemplate} from './reset_page.html.js';
import type {SettingsResetProfileDialogElement} from './reset_profile_dialog.js';

export interface SettingsResetPageElement {
  $: {
    resetProfileDialog: CrLazyRenderElement<SettingsResetProfileDialogElement>,
    resetProfile: HTMLElement,
  };
}

const SettingsResetPageElementBase = RouteObserverMixin(PolymerElement);

export class SettingsResetPageElement extends SettingsResetPageElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-reset-page';
  }

  static get template() {
    return getTemplate();
  }

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

  private onShowResetProfileDialog_() {
    Router.getInstance().navigateTo(
        routes.RESET_DIALOG, new URLSearchParams('origin=userclick'));
  }

  private onResetProfileDialogClose_() {
    Router.getInstance().navigateTo(routes.RESET_DIALOG.parent!);
    focusWithoutInk(this.$.resetProfile);
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
