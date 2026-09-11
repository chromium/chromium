// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../settings_page/settings_subpage.js';
import '../simple_confirmation_dialog.js';
import '../site_favicon.js';

import {assert} from '//resources/js/assert.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import {RouteObserverMixinLit} from '../router.js';
import type {Route} from '../router.js';
import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

import {GlicBrowserProxyImpl} from './glic_browser_proxy.js';
import type {GlicBrowserProxy, LoginPermission} from './glic_browser_proxy.js';
import {getCss} from './glic_login_permissions_page.css.js';
import {getHtml} from './glic_login_permissions_page.html.js';

const SettingsGlicLoginPermissionsPageElementBase =
    RouteObserverMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement)));

export type GlicLoginPermissionsPageElement =
    SettingsGlicLoginPermissionsPageElement;

export class SettingsGlicLoginPermissionsPageElement extends
    SettingsGlicLoginPermissionsPageElementBase {
  static get is() {
    return 'settings-glic-login-permissions-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      actorLoginPermissions_: {type: Array},
      isOnline_: {type: Boolean},
      selectedPermissionToRemove_: {type: Object},
    };
  }

  protected accessor actorLoginPermissions_: LoginPermission[] = [];
  protected accessor selectedPermissionToRemove_: LoginPermission|null = null;
  protected accessor isOnline_: boolean = navigator.onLine;

  private browserProxy_: GlicBrowserProxy = GlicBrowserProxyImpl.getInstance();
  private eventTracker_: EventTracker = new EventTracker();

  override currentRouteChanged(newRoute: Route, _oldRoute?: Route) {
    if (newRoute === routes.GEMINI_LOGIN) {
      this.browserProxy_.startObservingActorLoginPermissions();
      this.browserProxy_.getActorLoginPermissions().then(permissions => {
        this.actorLoginPermissions_ = permissions;
      });
    } else {
      this.browserProxy_.stopObservingActorLoginPermissions();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'actor-login-permissions-changed', (permissions: LoginPermission[]) => {
          this.actorLoginPermissions_ = permissions;
        });

    this.eventTracker_.add(window, 'online', () => this.isOnline_ = true);
    this.eventTracker_.add(window, 'offline', () => this.isOnline_ = false);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.browserProxy_.stopObservingActorLoginPermissions();
    this.eventTracker_.removeAll();
  }

  protected onRemoveActorLoginPermissionClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    this.selectedPermissionToRemove_ = this.actorLoginPermissions_[index];
  }

  protected async onRemoveDialogClose_() {
    const dialog =
        this.shadowRoot.querySelector<SettingsSimpleConfirmationDialogElement>(
            'settings-simple-confirmation-dialog');
    assert(dialog);
    assert(this.selectedPermissionToRemove_);
    if (dialog.wasConfirmed()) {
      const success = await this.browserProxy_.revokeActorLoginPermission(
          this.selectedPermissionToRemove_.signonRealm,
          this.selectedPermissionToRemove_.username);
      if (!success) {
        const toast =
            this.shadowRoot.querySelector<CrToastElement>('#removeErrorToast');
        assert(toast);
        toast.show();
      }
    }
    this.selectedPermissionToRemove_ = null;
  }

  protected getRemoveDialogDescription_(): string {
    assert(this.selectedPermissionToRemove_);
    return this.i18n(
        'glicRemoveActorLoginDialogDescription',
        this.selectedPermissionToRemove_.displayName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-login-permissions-page':
        SettingsGlicLoginPermissionsPageElement;
  }
}

customElements.define(
    SettingsGlicLoginPermissionsPageElement.is,
    SettingsGlicLoginPermissionsPageElement);
