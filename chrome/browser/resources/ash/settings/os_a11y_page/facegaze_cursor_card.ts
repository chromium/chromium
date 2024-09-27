// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'facegaze-cursor-card' is the card element containing facegaze cursor
 * settings.
 */

import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './facegaze_cursor_card.html.js';

const FaceGazeCursorCardElementBase = DeepLinkingMixin(RouteObserverMixin(
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface FaceGazeCursorCardElement {
  $: {};
}

export class FaceGazeCursorCardElement extends FaceGazeCursorCardElementBase {
  static get is() {
    return 'facegaze-cursor-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      syntheticCombinedCursorSpeedPref_: {
        type: Object,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            value: '',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            key: 'synthetic_combined_cursor_speed_pref',
          };
        },
      },

      minCursorSpeed: {
        type: Number,
        value: 5,
      },

      maxCursorSpeed: {
        type: Number,
        value: 50,
      },

      cursorSpeedTicks: {
        type: Object,
        value: [5, 10, 15, 20, 25, 30, 35, 40, 45, 50],
      },

      minCursorTuning: {
        type: Number,
        value: 4,
      },

      maxCursorTuning: {
        type: Number,
        value: 13,
      },

      tuningTicks: {
        type: Object,
        value: [4, 5, 6, 7, 8, 9, 10, 11, 12, 13],
      },
    };
  }

  static get observers() {
    return [
      'updateCombinedCursorSpeed_(syntheticCombinedCursorSpeedPref_.*)',
      'setCombinedCursorSpeed_(' +
          'prefs.settings.a11y.face_gaze.adjust_speed_separately.value)',
    ];
  }

  private syntheticCombinedCursorSpeedPref_:
      chrome.settingsPrivate.PrefObject<number>;

  constructor() {
    super();
  }

  override ready(): void {
    super.ready();

    this.setCombinedCursorSpeed_();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_FACEGAZE_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }

  private setCombinedCursorSpeed_(): void {
    if (this.get(
            'prefs.settings.a11y.face_gaze.adjust_speed_separately.value')) {
      return;
    }
    // When combined cursor speed is turned off, set the synthetic pref to the
    // default value.
    const value = loadTimeData.getInteger('defaultFaceGazeCursorSpeed');
    this.syntheticCombinedCursorSpeedPref_ = {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      key: 'synthetic_combined_cursor_speed_pref',
      value,
    };
  }

  private updateCombinedCursorSpeed_(): void {
    if (this.get(
            'prefs.settings.a11y.face_gaze.adjust_speed_separately.value')) {
      return;
    }
    // Set all 4 speeds to the same value.
    const speed = this.syntheticCombinedCursorSpeedPref_.value;
    this.setPrefValue('settings.a11y.face_gaze.cursor_speed_up', speed);
    this.setPrefValue('settings.a11y.face_gaze.cursor_speed_down', speed);
    this.setPrefValue('settings.a11y.face_gaze.cursor_speed_left', speed);
    this.setPrefValue('settings.a11y.face_gaze.cursor_speed_right', speed);
  }

  private onFaceGazeCursorResetButtonClick_(): void {
    this.setPrefValue('settings.a11y.face_gaze.adjust_speed_separately', false);
    this.setCombinedCursorSpeed_();
    this.setPrefValue(
        'settings.a11y.face_gaze.cursor_use_acceleration',
        loadTimeData.getBoolean('defaultFaceGazeCursorUseAcceleration'));
    this.setPrefValue(
        'settings.a11y.face_gaze.cursor_smoothing',
        loadTimeData.getInteger('defaultFaceGazeCursorSmoothing'));
    this.setPrefValue(
        'settings.a11y.face_gaze.velocity_threshold',
        loadTimeData.getInteger('defaultFaceGazeVelocityThreshold'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FaceGazeCursorCardElement.is]: FaceGazeCursorCardElement;
  }
}

customElements.define(FaceGazeCursorCardElement.is, FaceGazeCursorCardElement);
