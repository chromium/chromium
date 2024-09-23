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
import '../settings_page/settings_animated_pages.js';
import '../settings_shared.css.js';
// clang-format off
// <if expr="not is_macosx and not is_chromeos">
import './captions_subpage.js';
import '../settings_page/settings_subpage.js';
// </if>

// <if expr="is_win or is_linux or is_macosx">
import './ax_annotations_section.js';
// </if>

// <if expr="is_win or is_macosx">
import './live_caption_section.js';

import {CaptionsBrowserProxyImpl} from '/shared/settings/a11y_page/captions_browser_proxy.js';
// </if>
// clang-format on
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import type {AccessibilityBrowserProxy} from './a11y_browser_proxy.js';
import {AccessibilityBrowserProxyImpl} from './a11y_browser_proxy.js';
import {getTemplate} from './a11y_page.html.js';

// clang-format off
// <if expr="not is_chromeos">
import type {LanguageHelper, LanguagesModel} from '../languages_page/languages_types.js';

// </if>
// clang-format on

const SettingsA11yPageElementBase =
    PrefsMixin(WebUiListenerMixin(BaseMixin(PolymerElement)));

export class SettingsA11yPageElement extends SettingsA11yPageElementBase {
  static get is() {
    return 'settings-a11y-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current active route.
       */
      currentRoute: {
        type: Object,
        notify: true,
      },

      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      // <if expr="not is_chromeos">
      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,

      enableLiveCaption_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveCaption');
        },
      },
      // </if>

      /**
       * Indicate whether a screen reader is enabled. Also, determine whether
       * to show accessibility labels settings.
       */
      hasScreenReader_: {
        type: Boolean,
        value: false,
      },

      // <if expr="is_win or is_linux or is_macosx">
      /**
       * Whether to show the AxAnnotations subpage.
       */
      showAxAnnotationsSection_: {
        type: Boolean,
        computed: 'computeShowAxAnnotationsSection_(hasScreenReader_)',
      },
      // </if>

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.CAPTIONS) {
            map.set(routes.CAPTIONS.path, '#captions');
          }
          return map;
        },
      },

      /**
       * Whether the caption settings link opens externally.
       */
      captionSettingsOpensExternally_: {
        type: Boolean,
        value() {
          let opensExternally = false;
          // <if expr="is_macosx or is_win">
          opensExternally = true;
          // </if>
          return opensExternally;
        },
      },

      /**
       * Whether to show the overscroll history navigation setting.
       */
      showOverscrollHistoryNavigationToggle_: {
        type: Boolean,
        value: function() {
          let showOverscroll = false;
          // <if expr="is_win or is_linux or is_macosx">
          showOverscroll = true;
          // </if>
          return showOverscroll;
        },
      },
    };
  }

  private browserProxy_: AccessibilityBrowserProxy =
      AccessibilityBrowserProxyImpl.getInstance();

  // <if expr="not is_chromeos">
  languages: LanguagesModel;
  languageHelper: LanguageHelper;

  private enableLiveCaption_: boolean;
  // </if>

  private captionSettingsOpensExternally_: boolean;
  private hasScreenReader_: boolean;
  private showOverscrollHistoryNavigationToggle_: boolean;
  // <if expr="is_win or is_linux or is_macosx">
  private showAxAnnotationsSection_: boolean;
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

  private onA11yCaretBrowsingChange_(event: Event) {
    if ((event.target as SettingsToggleButtonElement).checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
  }

  private onA11yImageLabelsChange_(event: Event) {
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
  // </if>

  // <if expr="not is_chromeos">
  private onFocusHighlightChange_(event: Event) {
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.FocusHighlight.ToggleEnabled',
        (event.target as SettingsToggleButtonElement).checked);
  }
  // </if>

  // <if expr="is_chromeos">
  private onManageSystemAccessibilityFeaturesClick_() {
    window.location.href = 'chrome://os-settings/osAccessibility';
  }
  // </if>

  /** private */
  private onMoreFeaturesLinkClick_() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/3p_accessibility_extensions');
  }

  private onCaptionsClick_() {
    if (this.captionSettingsOpensExternally_) {
      // <if expr="is_win or is_macosx">
      CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
      // </if>
    } else {
      Router.getInstance().navigateTo(routes.CAPTIONS);
    }
  }

  // <if expr="is_win or is_linux">
  private onOverscrollHistoryNavigationChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.browserProxy_.recordOverscrollHistoryNavigationChanged(enabled);
  }
  // </if>

  // <if expr="is_macosx">
  private onMacTrackpadGesturesLinkClick_() {
    this.browserProxy_.openTrackpadGesturesSettings();
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsA11yPageElement.is]: SettingsA11yPageElement;
  }
}

customElements.define(SettingsA11yPageElement.is, SettingsA11yPageElement);
