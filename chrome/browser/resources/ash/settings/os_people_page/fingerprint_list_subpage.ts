// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './setup_fingerprint_dialog.js';
import '../settings_shared.css.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {focusWithoutInk} from 'chrome://resources/ash/common/focus_without_ink_js.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists, castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {FingerprintBrowserProxy, FingerprintBrowserProxyImpl, FingerprintInfo} from './fingerprint_browser_proxy.js';
import {getTemplate} from './fingerprint_list_subpage.html.js';

const SettingsFingerprintListSubpageElementBase = RouteObserverMixin(
    WebUiListenerMixin(I18nMixin(DeepLinkingMixin(PolymerElement))));

export class SettingsFingerprintListSubpageElement extends
    SettingsFingerprintListSubpageElementBase {
  static get is() {
    return 'settings-fingerprint-list-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Authentication token provided by settings-people-page.
       */
      authToken: {
        type: String,
        notify: true,
        observer: 'onAuthTokenChanged_',
      },

      fingerprints_: {
        type: Array,
        value() {
          return [];
        },
      },

      showSetupFingerprintDialog_: Boolean,

      allowAddAnotherFinger_: {
        type: Boolean,
        value: true,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kAddFingerprintV2,
          Setting.kRemoveFingerprintV2,
        ]),
      },
    };
  }

  authToken: string|undefined;
  private fingerprints_: string[];
  private showSetupFingerprintDialog_: boolean;
  private allowAddAnotherFinger_: boolean;
  private browserProxy_: FingerprintBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = FingerprintBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener('on-screen-locked', this.onScreenLocked_.bind(this));
    this.updateFingerprintsList_();
  }

  /**
   * @return whether an event was fired to show the password dialog.
   */
  private requestPasswordIfApplicable_(): boolean {
    const currentRoute = Router.getInstance().currentRoute;
    if (currentRoute === routes.FINGERPRINT && !this.authToken) {
      const event = new CustomEvent(
          'password-requested', {bubbles: true, composed: true});
      this.dispatchEvent(event);
      return true;
    }
    return false;
  }

  override currentRouteChanged(newRoute: Route): void {
    if (newRoute !== routes.FINGERPRINT) {
      this.showSetupFingerprintDialog_ = false;
      return;
    }

    if (this.requestPasswordIfApplicable_()) {
      this.showSetupFingerprintDialog_ = false;
    }

    this.attemptDeepLink();
  }

  private updateFingerprintsList_(): void {
    this.browserProxy_.getFingerprintsList().then(
        this.onFingerprintsChanged_.bind(this));
  }

  private onFingerprintsChanged_(fingerprintInfo: FingerprintInfo): void {
    // Update iron-list.
    this.fingerprints_ = fingerprintInfo.fingerprintsList.slice();
    this.shadowRoot!.querySelector<CrButtonElement>(
                        '.action-button')!.disabled = fingerprintInfo.isMaxed;
    this.allowAddAnotherFinger_ = !fingerprintInfo.isMaxed;
  }

  private onFingerprintDeleteTapped_(e: DomRepeatEvent<number>): void {
    assertExists(this.authToken);
    this.browserProxy_.removeEnrollment(e.model.index, this.authToken)
        .then(success => {
          if (success) {
            recordSettingChange(Setting.kRemoveFingerprintV2);
            this.updateFingerprintsList_();
          }
        });
  }

  private onFingerprintLabelChanged_(e: DomRepeatEvent<string>): void {
    this.browserProxy_.changeEnrollmentLabel(e.model.index, e.model.item)
        .then(success => {
          if (success) {
            this.updateFingerprintsList_();
          }
        });
  }

  private openAddFingerprintDialog_(): void {
    this.showSetupFingerprintDialog_ = true;
  }

  private onSetupFingerprintDialogClose_(): void {
    this.showSetupFingerprintDialog_ = false;
    focusWithoutInk(
        castExists(this.shadowRoot!.querySelector('#addFingerprint')));
  }

  /**
   * Close the setup fingerprint dialog when the screen is unlocked.
   */
  private onScreenLocked_(screenIsLocked: boolean): void {
    if (!screenIsLocked &&
        Router.getInstance().currentRoute === routes.FINGERPRINT) {
      this.onSetupFingerprintDialogClose_();
    }
  }

  private onAuthTokenChanged_(): void {
    if (this.requestPasswordIfApplicable_()) {
      this.showSetupFingerprintDialog_ = false;
      return;
    }

    if (Router.getInstance().currentRoute === routes.FINGERPRINT) {
      // Show deep links again if the user authentication dialog just closed.
      this.attemptDeepLink();
    }
  }

  private getButtonAriaLabel_(item: string): string {
    return this.i18n('lockScreenDeleteFingerprintLabel', item);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsFingerprintListSubpageElement.is]:
        SettingsFingerprintListSubpageElement;
  }
}

customElements.define(
    SettingsFingerprintListSubpageElement.is,
    SettingsFingerprintListSubpageElement);
