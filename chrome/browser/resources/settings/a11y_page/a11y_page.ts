// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';

import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

// clang-format off

// <if expr="is_win or is_linux or is_macosx">
import './ax_annotations_section.js';
// </if>
// <if expr="is_win or is_macosx">
import './live_caption.js';

import {CaptionsBrowserProxyImpl} from '/shared/settings/a11y_page/captions_browser_proxy.js';
// </if>
// clang-format on
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {AccessibilityBrowserProxy} from './a11y_browser_proxy.js';
import {AccessibilityBrowserProxyImpl} from './a11y_browser_proxy.js';
import {getHtml} from './a11y_page.html.js';


/**
 * Must be kept in sync with the C++ enum of the same name in
 * chrome/browser/ui/toasts/toast_metrics.h.
 */
export enum ToastAlertLevel {
  ALL = 0,
  ACTIONABLE = 1,
  // Must be last.
  COUNT = 1,
}

const SettingsA11yPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(CrLitElement));

// <if expr="not is_chromeos">
export interface SettingsA11yPageElement {
  $: {
    toastToggle: SettingsToggleButtonElement,
  };
}
// </if>

export class SettingsA11yPageElement extends SettingsA11yPageElementBase {
  static get is() {
    return 'settings-a11y-page';
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
      // <if expr="not is_chromeos">
      enableLiveCaption_: {type: Boolean},
      numericUncheckedToastAlertValues_: {type: Array},
      // </if>

      /**
       * Indicate whether a screen reader is enabled. Also, determine whether
       * to show accessibility labels settings.
       */
      hasScreenReader_: {type: Boolean},

      /**
       * Whether to show the AxTreeFixing subpage.
       */
      showAxTreeFixingSection_: {type: Boolean},

      // <if expr="is_win or is_linux or is_macosx">
      /**
       * Whether to show the AxAnnotations subpage.
       */
      showAxAnnotationsSection_: {type: Boolean},
      // </if>
    };
  }

  private browserProxy_: AccessibilityBrowserProxy =
      AccessibilityBrowserProxyImpl.getInstance();

  // <if expr="not is_chromeos">
  protected accessor enableLiveCaption_: boolean =
      loadTimeData.getBoolean('enableLiveCaption');
  protected accessor numericUncheckedToastAlertValues_: ToastAlertLevel[] =
      [ToastAlertLevel.ACTIONABLE];
  // </if>

  protected accessor hasScreenReader_: boolean = false;
  protected accessor showAxTreeFixingSection_: boolean =
      loadTimeData.getBoolean('axTreeFixingEnabled');
  // <if expr="is_win or is_linux or is_macosx">
  protected accessor showAxAnnotationsSection_: boolean = false;
  // </if>

  override connectedCallback() {
    super.connectedCallback();

    const updateScreenReaderState = (hasScreenReader: boolean) => {
      this.hasScreenReader_ = hasScreenReader;
    };
    this.browserProxy_.getScreenReaderState().then(updateScreenReaderState);
    this.addWebUiListener(
        'screen-reader-state-changed', updateScreenReaderState);
  }

  // <if expr="is_win or is_linux or is_macosx">
  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('hasScreenReader_')) {
      this.showAxAnnotationsSection_ = this.computeShowAxAnnotationsSection_();
    }
  }
  // </if>

  protected onA11yCaretBrowsingChange_(event: Event) {
    if ((event.target as SettingsToggleButtonElement).checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
  }

  protected onA11yImageLabelsChange_(event: Event) {
    const a11yImageLabelsOn =
        (event.target as SettingsToggleButtonElement).checked;
    if (a11yImageLabelsOn) {
      chrome.send('confirmA11yImageLabels');
    }
  }

  // <if expr="is_win or is_linux or is_macosx">
  /**
   * Return whether to show the AxAnnotations subpage based on:
   *    1. If any annotation's feature flag is enabled.
   *    2. Whether a screen reader is enabled.
   * Note: on ChromeOS, the AxAnnotations subpage is shown on a different
   * settings page; i.e. Settings > Accessibility > Text-to-Speech.
   */
  private computeShowAxAnnotationsSection_(): boolean {
    const anyAxAnnotationsFeatureEnabled =
        loadTimeData.getBoolean('mainNodeAnnotationsEnabled');
    return anyAxAnnotationsFeatureEnabled && this.hasScreenReader_;
  }

  protected onCaptionsClick_() {
    // <if expr="is_win or is_macosx">
    CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
    // </if>
    // <if expr="is_linux">
    Router.getInstance().navigateTo(routes.CAPTIONS);
    // </if>
  }
  // </if>

  // <if expr="not is_chromeos">
  protected onFocusHighlightSettingBooleanControlChange_(event: Event) {
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.FocusHighlight.ToggleEnabled',
        (event.target as SettingsToggleButtonElement).checked);
  }
  // </if>

  // <if expr="is_chromeos">
  protected onManageSystemAccessibilityFeaturesClick_() {
    window.location.href = 'chrome://os-settings/osAccessibility';
  }
  // </if>

  /** private */
  protected onMoreFeaturesLinkClick_() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/3p_accessibility_extensions');
  }

  // <if expr="is_win or is_linux">
  protected onOverscrollHistoryNavigationChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.browserProxy_.recordOverscrollHistoryNavigationChanged(enabled);
  }
  // </if>

  // <if expr="is_macosx">
  protected onMacTrackpadGesturesLinkClick_() {
    this.browserProxy_.openTrackpadGesturesSettings();
  }
  // </if>

  // <if expr="not is_chromeos">
  protected onToastAlertLevelChange_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'Toast.FrequencyPrefChanged',
        PrefService.getInstance()
            .getPref<number>('settings.toast.alert_level')
            .value,
        ToastAlertLevel.COUNT);
  }
  // </if>

  // <if expr="is_linux">
  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();
    if (routes.CAPTIONS) {
      map.set(routes.CAPTIONS.path, '#captions');
    }
    return map;
  }
  // </if>

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    assert(childViewId === 'captions');
    const control = this.shadowRoot.querySelector<HTMLElement>('#captions');
    assert(
        control,
        `Failed to find associated control for child '${childViewId}'`);
    return control;
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'settings-a11y-page': SettingsA11yPageElement;
  }
}

customElements.define(SettingsA11yPageElement.is, SettingsA11yPageElement);
