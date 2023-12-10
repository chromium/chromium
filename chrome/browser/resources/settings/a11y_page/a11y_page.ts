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
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_shared.css.js';
// clang-format off
// <if expr="not is_macosx and not is_chromeos">
import './captions_subpage.js';
import '../settings_page/settings_subpage.js';
// </if>

// <if expr="is_win or is_macosx">
import './live_caption_section.js';

import {CaptionsBrowserProxyImpl} from '/shared/settings/a11y_page/captions_browser_proxy.js';
// </if>
// clang-format on
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {AccessibilityBrowserProxy, AccessibilityBrowserProxyImpl} from './a11y_browser_proxy.js';
import {getTemplate} from './a11y_page.html.js';

// clang-format off
// <if expr="not is_chromeos">
import {LanguageHelper, LanguagesModel} from '../languages_page/languages_types.js';

// </if>
// clang-format on

// TODO(crbug.com/1442928): Encapsulate all PDF OCR toggle logic to a dedicated
// pdf-ocr-toggle-button element.
// <if expr="is_win or is_linux or is_macosx">
/**
 * Numerical values should not be changed because they must stay in sync with
 * screen_ai::ScreenAIInstallState::State defined in screen_ai_install_state.h.
 */
export enum ScreenAiInstallStatus {
  NOT_DOWNLOADED = 0,
  DOWNLOADING = 1,
  FAILED = 2,
  DOWNLOADED = 3,
  READY = 4,
}
// </if>

const SettingsA11yPageElementBase =
    PrefsMixin(WebUiListenerMixin(I18nMixin(BaseMixin(PolymerElement))));

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

      /**
       * Whether to show the focus highlight setting.
       * Depends on feature flag for focus highlight.
       */
      showFocusHighlightOption_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('showFocusHighlightOption');
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
       * `pdfOcrProgress_` stores the downloading progress in percentage of
       * the ScreenAI library, which ranges from 0.0 to 100.0.
       */
      pdfOcrProgress_: Number,

      /**
       * `pdfOcrStatus_` stores the ScreenAI library install state.
       */
      pdfOcrStatus_: Number,

      /**
       * Whether to show pdf ocr settings.
       */
      showPdfOcrToggle_: {
        type: Boolean,
        computed: 'computeShowPdfOcrToggle_(hasScreenReader_)',
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
          // <if expr="is_macosx">
          opensExternally = true;
          // </if>

          // <if expr="is_win">
          opensExternally = loadTimeData.getBoolean('isWindows10OrNewer');
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
          showOverscroll = loadTimeData.getBoolean(
              'overscrollHistoryNavigationSettingEnabled');
          // </if>
          return showOverscroll;
        },
      },
    };
  }

  private accessibilityBrowserProxy: AccessibilityBrowserProxy =
      AccessibilityBrowserProxyImpl.getInstance();

  // <if expr="not is_chromeos">
  languages: LanguagesModel;
  languageHelper: LanguageHelper;

  private enableLiveCaption_: boolean;
  private showFocusHighlightOption_: boolean;
  // </if>

  private captionSettingsOpensExternally_: boolean;
  private hasScreenReader_: boolean;
  private showOverscrollHistoryNavigationToggle_: boolean;

  // <if expr="is_win or is_linux or is_macosx">
  private pdfOcrProgress_: number;
  private pdfOcrStatus_: ScreenAiInstallStatus;
  private showPdfOcrToggle_: boolean;
  // </if>

  override ready() {
    super.ready();

    this.addWebUiListener(
        'screen-reader-state-changed', (hasScreenReader: boolean) => {
          this.hasScreenReader_ = hasScreenReader;
        });
    // <if expr="is_win or is_linux or is_macosx">
    if (loadTimeData.getBoolean('pdfOcrEnabled')) {
      this.addWebUiListener(
          'pdf-ocr-state-changed', (pdfOcrState: ScreenAiInstallStatus) => {
            this.pdfOcrStatus_ = pdfOcrState;
          });
      this.addWebUiListener(
          'pdf-ocr-downloading-progress-changed', (progress: number) => {
            this.pdfOcrProgress_ = progress;
          });
    }
    // </if>

    // Enables javascript and gets the screen reader state.
    chrome.send('a11yPageReady');
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
  private getPdfOcrToggleSublabel_(): string {
    switch (this.pdfOcrStatus_) {
      case ScreenAiInstallStatus.DOWNLOADING:
        return this.pdfOcrProgress_ > 0 && this.pdfOcrProgress_ < 100 ?
            this.i18n('pdfOcrDownloadProgressLabel', this.pdfOcrProgress_) :
            this.i18n('pdfOcrDownloadingLabel');
      case ScreenAiInstallStatus.FAILED:
        return this.i18n('pdfOcrDownloadErrorLabel');
      case ScreenAiInstallStatus.DOWNLOADED:
        return this.i18n('pdfOcrDownloadCompleteLabel');
      case ScreenAiInstallStatus.READY:  // fallthrough
      case ScreenAiInstallStatus.NOT_DOWNLOADED:
        // No subtitle update, so show a generic subtitle describing PDF OCR.
        return this.i18n('pdfOcrSubtitle');
    }
  }

  /**
   * Return whether to show a PDF OCR toggle button based on:
   *    1. A PDF OCR feature flag is enabled.
   *    2. Whether a screen reader is enabled.
   * Note: on ChromeOS, the PDF OCR toggle is shown on a different settings
   * page; i.e. Settings > Accessibility > Text-to-Speech.
   */
  private computeShowPdfOcrToggle_(): boolean {
    return loadTimeData.getBoolean('pdfOcrEnabled') && this.hasScreenReader_;
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
    this.accessibilityBrowserProxy.recordOverscrollHistoryNavigationChanged(
        enabled);
  }
  // </if>

  // <if expr="is_macosx">
  private onMacTrackpadGesturesLinkClick_() {
    this.accessibilityBrowserProxy.openTrackpadGesturesSettings();
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsA11yPageElement.is]: SettingsA11yPageElement;
  }
}

customElements.define(SettingsA11yPageElement.is, SettingsA11yPageElement);
