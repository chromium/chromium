// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'date-time-settings-card' is the card element containing date and time
 * settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';
import '../controls/settings_toggle_button.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import './timezone_selector.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isChild, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {DateTimeBrowserProxy, DateTimePageCallbackRouter, DateTimePageHandlerRemote} from './date_time_browser_proxy.js';
import {getTemplate} from './date_time_settings_card.html.js';

const DateTimeSettingsCardElementBase =
    DeepLinkingMixin(RouteOriginMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class DateTimeSettingsCardElement extends
    DateTimeSettingsCardElementBase {
  static get is() {
    return 'date-time-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeTimeZoneDisplayName: {
        type: String,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.k24HourClock,
          Setting.kChangeTimeZone,
        ]),
      },

      /**
       * Whether date and time are settable. Normally the date and time are
       * forced by network time, so default to false to initially hide the
       * button.
       */
      canSetDateTime_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the icon informing that this action is managed by a parent is
       * displayed.
       */
      shouldShowManagedByParentIcon_: {
        type: Boolean,
        value: () => {
          return isChild();
        },
      },

      timeZoneSettingSublabel_: {
        type: String,
        computed: `computeTimeZoneSettingSublabel_(
            activeTimeZoneDisplayName,
            prefs.generated.resolve_timezone_by_geolocation_on_off.value,
            prefs.generated.resolve_timezone_by_geolocation_method_short.value)`,
      },

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              timezone: 'os-settings:clock',
              use24hour: 'os-settings:24hour',
              setDateTime: 'os-settings:set-date-time',
            };
          }
          return {
            timezone: '',
            use24hour: '',
            setDateTime: '',
          };
        },
      },
    };
  }

  activeTimeZoneDisplayName: string;
  private canSetDateTime_: boolean;
  private rowIcons_: Record<string, string>;
  private shouldShowManagedByParentIcon_: boolean;
  private timeZoneSettingSublabel_: string;

  /**
   * Returns the browser proxy page handler (to invoke functions).
   */
  get pageHandler(): DateTimePageHandlerRemote {
    return DateTimeBrowserProxy.getInstance().handler;
  }

  /**
   * Returns the browser proxy callback router (to receive async messages).
   */
  get callbackRouter(): DateTimePageCallbackRouter {
    return DateTimeBrowserProxy.getInstance().observer;
  }

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = isRevampWayfindingEnabled() ? routes.SYSTEM_PREFERENCES :
                                               routes.DATETIME;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.callbackRouter.onSystemClockCanSetTimeChanged.addListener(
        this.onCanSetDateTimeChanged_.bind(this));
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(
        routes.DATETIME_TIMEZONE_SUBPAGE, '#timeZoneSettingsTrigger');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private onCanSetDateTimeChanged_(canSetDateTime: boolean): void {
    this.canSetDateTime_ = canSetDateTime;
  }

  private onSetDateTimeClick_(): void {
    this.pageHandler.showSetDateTimeUI();
  }

  private openTimeZoneSubpage_(): void {
    Router.getInstance().navigateTo(routes.DATETIME_TIMEZONE_SUBPAGE);
  }

  private computeTimeZoneSettingSublabel_(): string {
    // Note: `this.getPref()` will assert the queried pref exists, but the prefs
    // property may not be initialized yet when this element runs the first
    // computation of this method. Ensure prefs is initialized first.
    if (!this.prefs) {
      return '';
    }

    if (!this.getPref('generated.resolve_timezone_by_geolocation_on_off')
             .value) {
      return this.activeTimeZoneDisplayName;
    }
    const method =
        this.getPref<number>(
                'generated.resolve_timezone_by_geolocation_method_short')
            .value;
    const id = [
      'setTimeZoneAutomaticallyDisabled',
      'setTimeZoneAutomaticallyIpOnlyDefault',
      'setTimeZoneAutomaticallyWithWiFiAccessPointsData',
      'setTimeZoneAutomaticallyWithAllLocationInfo',
    ][method];
    return id ? this.i18n(id) : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DateTimeSettingsCardElement.is]: DateTimeSettingsCardElement;
  }
}

customElements.define(
    DateTimeSettingsCardElement.is, DateTimeSettingsCardElement);
