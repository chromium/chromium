// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-guide-page' is the settings page that helps users guide
 * various privacy settings.
 */
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './privacy_guide_completion_fragment.js';
import './privacy_guide_cookies_fragment.js';
import './privacy_guide_history_sync_fragment.js';
import './privacy_guide_msbb_fragment.js';
import './privacy_guide_safe_browsing_fragment.js';
import './privacy_guide_search_suggestions_fragment.js';
import './privacy_guide_welcome_fragment.js';
import './step_indicator.js';

import {SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrSettingsPrefs} from 'chrome://resources/cr_components/settings_prefs/prefs_types.js';
import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../../hats_browser_proxy.js';
import {loadTimeData} from '../../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';
import {NetworkPredictionOptions} from '../../performance_page/constants.js';
import {SafeBrowsingSetting} from '../../privacy_page/security_page.js';
import {routes} from '../../route.js';
import {Route, RouteObserverMixin, Router} from '../../router.js';
import {CookiePrimarySetting} from '../../site_settings/site_settings_prefs_browser_proxy.js';

import {PrivacyGuideStep, PrivacyGuideStepPg3Off} from './constants.js';
import {PrivacyGuideAvailabilityMixin} from './privacy_guide_availability_mixin.js';
import {getTemplate} from './privacy_guide_page.html.js';
import {StepIndicatorModel} from './step_indicator.js';

interface PrivacyGuideStepComponents {
  nextStep?: PrivacyGuideStep;
  onForwardNavigation?(): void;
  previousStep?: PrivacyGuideStep;
  onBackwardNavigation?(): void;
  isAvailable(): boolean;
}

function eligibilityToRecord(step: PrivacyGuideStep):
    PrivacyGuideStepsEligibleAndReached {
  switch (step) {
    case PrivacyGuideStep.MSBB:
      return PrivacyGuideStepsEligibleAndReached.MSBB_ELIGIBLE;
    case PrivacyGuideStep.HISTORY_SYNC:
      return PrivacyGuideStepsEligibleAndReached.HISTORY_SYNC_ELIGIBLE;
    case PrivacyGuideStep.COOKIES:
      return PrivacyGuideStepsEligibleAndReached.COOKIES_ELIGIBLE;
    case PrivacyGuideStep.SAFE_BROWSING:
      return PrivacyGuideStepsEligibleAndReached.SAFE_BROWSING_ELIGIBLE;
    case PrivacyGuideStep.SEARCH_SUGGESTIONS:
      return PrivacyGuideStepsEligibleAndReached.SEARCH_SUGGESTIONS_ELIGIBLE;
    case PrivacyGuideStep.COMPLETION:
      return PrivacyGuideStepsEligibleAndReached.COMPLETION_ELIGIBLE;
    default:
      assertNotReached();
  }
}

export interface SettingsPrivacyGuidePageElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const PrivacyGuideBase = RouteObserverMixin(PrivacyGuideAvailabilityMixin(
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement)))));

export class SettingsPrivacyGuidePageElement extends PrivacyGuideBase {
  static get is() {
    return 'settings-privacy-guide-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Valid privacy guide states.
       */
      privacyGuideStepEnum_: {
        type: Object,
        value: PrivacyGuideStep,
      },

      /**
       * The current step in the privacy guide flow, or `undefined` if the flow
       * has not yet been initialized from query parameters.
       */
      privacyGuideStep_: {
        type: String,
        value: undefined,
      },

      /**
       * Multiplier to apply on translate distances for animations in fragments.
       * +1 if navigating forwards LTR or backwards RTL; -1 if navigating
       * forwards RTL or backwards LTR.
       */
      translateMultiplier_: {
        type: Number,
        value: 1,
      },

      /**
       * Used by the 'step-indicator' element to display its dots.
       */
      stepIndicatorModel_: {
        type: Object,
        computed:
            'computeStepIndicatorModel(privacyGuideStep_, prefs.generated.cookie_primary_setting, prefs.generated.safe_browsing, prefs.net.network_prediction_options)',
      },

      syncStatus_: Object,
    };
  }

  static get observers() {
    return [
      'onPrefsChanged_(prefs.generated.cookie_primary_setting, prefs.generated.safe_browsing, prefs.net.network_prediction_options)',
      'exitIfNecessary(isPrivacyGuideAvailable)',
    ];
  }

  private privacyGuideStep_: PrivacyGuideStep;
  private stepIndicatorModel_: StepIndicatorModel;
  private privacyGuideStepToComponentsMap_:
      Map<PrivacyGuideStep, PrivacyGuideStepComponents>;
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private syncStatus_: SyncStatus;
  private animationsEnabled_: boolean = true;
  private translateMultiplier_: number;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  constructor() {
    super();

    this.privacyGuideStepToComponentsMap_ =
        this.computePrivacyGuideStepToComponentsMap_();
  }

  override ready() {
    super.ready();

    this.addWebUiListener(
        'sync-status-changed',
        (syncStatus: SyncStatus) => this.onSyncStatusChanged_(syncStatus));
    this.syncBrowserProxy_.getSyncStatus().then(
        (syncStatus: SyncStatus) => this.onSyncStatusChanged_(syncStatus));
  }

  disableAnimationsForTesting() {
    this.animationsEnabled_ = false;
  }

  /** RouteObserverBehavior */
  override currentRouteChanged(newRoute: Route) {
    if (newRoute !== routes.PRIVACY_GUIDE || this.exitIfNecessary()) {
      return;
    }
    this.updateStateFromQueryParameters_();
  }

  /**
   * @return the map of privacy guide steps to their components.
   */
  private computePrivacyGuideStepToComponentsMap_():
      Map<PrivacyGuideStep, PrivacyGuideStepComponents> {
    if (loadTimeData.getBoolean('enablePrivacyGuide3')) {
      return new Map([
        [
          PrivacyGuideStep.WELCOME,
          {
            nextStep: PrivacyGuideStep.MSBB,
            isAvailable: () => true,
            onForwardNavigation: () => {
              this.metricsBrowserProxy_
                  .recordPrivacyGuideNextNavigationHistogram(
                      PrivacyGuideInteractions.WELCOME_NEXT_BUTTON);
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.NextClickWelcome');
              this.metricsBrowserProxy_.recordPrivacyGuideFlowLengthHistogram(
                  this.computeStepIndicatorModel().total);
              this.recordEligibleSteps_();
            },
          },
        ],
        [
          PrivacyGuideStep.MSBB,
          {
            nextStep: PrivacyGuideStep.HISTORY_SYNC,
            previousStep: PrivacyGuideStep.WELCOME,
            onForwardNavigation: () => {
              this.metricsBrowserProxy_
                  .recordPrivacyGuideNextNavigationHistogram(
                      PrivacyGuideInteractions.MSBB_NEXT_BUTTON);
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.NextClickMSBB');
            },
            onBackwardNavigation: () => {
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.BackClickMSBB');
            },
            isAvailable: () => true,
          },
        ],
        [
          PrivacyGuideStep.HISTORY_SYNC,
          {
            nextStep: PrivacyGuideStep.COOKIES,
            previousStep: PrivacyGuideStep.MSBB,
            onForwardNavigation: () => {
              this.metricsBrowserProxy_
                  .recordPrivacyGuideNextNavigationHistogram(
                      PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON);
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.NextClickHistorySync');
            },
            onBackwardNavigation: () => {
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.BackClickHistorySync');
            },
            // Allow the history sync card to be shown while `syncStatus_` is
            // unavailable. Otherwise we would skip it in
            // `navigateForwardIfCurrentCardNoLongerAvailable` before
            // `onSyncStatusChanged_` is called asynchronously.
            isAvailable: () => !this.syncStatus_ || this.isSyncOn_(),
          },
        ],
        [
          PrivacyGuideStep.COOKIES,
          {
            nextStep: PrivacyGuideStep.SAFE_BROWSING,
            onForwardNavigation: () => {
              this.metricsBrowserProxy_
                  .recordPrivacyGuideNextNavigationHistogram(
                      PrivacyGuideInteractions.COOKIES_NEXT_BUTTON);
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.NextClickCookies');
            },
            onBackwardNavigation: () => {
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.BackClickCookies');
            },

            previousStep: PrivacyGuideStep.HISTORY_SYNC,
            isAvailable: () => this.shouldShowCookiesCard_(),
          },
        ],
        [
          PrivacyGuideStep.SAFE_BROWSING,
          {
            nextStep: PrivacyGuideStep.SEARCH_SUGGESTIONS,
            previousStep: PrivacyGuideStep.COOKIES,
            onForwardNavigation: () => {
              this.metricsBrowserProxy_
                  .recordPrivacyGuideNextNavigationHistogram(
                      PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON);
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.NextClickSafeBrowsing');
            },
            onBackwardNavigation: () => {
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.BackClickSafeBrowsing');
            },
            isAvailable: () => this.shouldShowSafeBrowsingCard_(),
          },
        ],
        [
          PrivacyGuideStep.SEARCH_SUGGESTIONS,
          {
            nextStep: PrivacyGuideStep.PRELOAD,
            previousStep: PrivacyGuideStep.SAFE_BROWSING,
            onForwardNavigation: () => {
              this.metricsBrowserProxy_
                  .recordPrivacyGuideNextNavigationHistogram(
                      PrivacyGuideInteractions.SEARCH_SUGGESTIONS_NEXT_BUTTON);
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.NextClickSearchSuggestions');
            },
            onBackwardNavigation: () => {
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.BackClickSearchSuggestions');
            },
            isAvailable: () => true,
          },
        ],
        [
          PrivacyGuideStep.PRELOAD,
          {
            nextStep: PrivacyGuideStep.COMPLETION,
            previousStep: PrivacyGuideStep.SEARCH_SUGGESTIONS,
            onForwardNavigation: () => {
              HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
                  TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE);
            },
            isAvailable: () => this.shouldShowPreloadCard_(),
          },
        ],
        [
          PrivacyGuideStep.COMPLETION,
          {
            onBackwardNavigation: () => {
              this.metricsBrowserProxy_.recordAction(
                  'Settings.PrivacyGuide.BackClickCompletion');
            },
            previousStep: PrivacyGuideStep.PRELOAD,
            isAvailable: () => true,
          },
        ],
      ]);
    }
    return new Map([
      [
        PrivacyGuideStep.WELCOME,
        {
          nextStep: PrivacyGuideStep.MSBB,
          isAvailable: () => true,
          onForwardNavigation: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.WELCOME_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickWelcome');
            this.metricsBrowserProxy_.recordPrivacyGuideFlowLengthHistogram(
                this.computeStepIndicatorModel().total);
            this.recordEligibleSteps_();
          },
        },
      ],
      [
        PrivacyGuideStep.MSBB,
        {
          nextStep: PrivacyGuideStep.HISTORY_SYNC,
          previousStep: PrivacyGuideStep.WELCOME,
          onForwardNavigation: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.MSBB_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickMSBB');
          },
          onBackwardNavigation: () => {
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.BackClickMSBB');
          },
          isAvailable: () => true,
        },
      ],
      [
        PrivacyGuideStep.HISTORY_SYNC,
        {
          nextStep: PrivacyGuideStep.SAFE_BROWSING,
          previousStep: PrivacyGuideStep.MSBB,
          onForwardNavigation: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickHistorySync');
          },
          onBackwardNavigation: () => {
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.BackClickHistorySync');
          },
          // Allow the history sync card to be shown while `syncStatus_` is
          // unavailable. Otherwise we would skip it in
          // `navigateForwardIfCurrentCardNoLongerAvailable` before
          // `onSyncStatusChanged_` is called asynchronously.
          isAvailable: () => !this.syncStatus_ || this.isSyncOn_(),
        },
      ],
      [
        PrivacyGuideStep.SAFE_BROWSING,
        {
          nextStep: PrivacyGuideStep.COOKIES,
          previousStep: PrivacyGuideStep.HISTORY_SYNC,
          onForwardNavigation: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickSafeBrowsing');
          },
          onBackwardNavigation: () => {
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.BackClickSafeBrowsing');
          },
          isAvailable: () => this.shouldShowSafeBrowsingCard_(),
        },
      ],
      [
        PrivacyGuideStep.COOKIES,
        {
          nextStep: PrivacyGuideStep.COMPLETION,
          onForwardNavigation: () => {
            HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
                TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE);
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.COOKIES_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickCookies');
          },
          onBackwardNavigation: () => {
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.BackClickCookies');
          },
          previousStep: PrivacyGuideStep.SAFE_BROWSING,
          isAvailable: () => this.shouldShowCookiesCard_(),
        },
      ],
      [
        PrivacyGuideStep.COMPLETION,
        {
          onBackwardNavigation: () => {
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.BackClickCompletion');
          },
          previousStep: PrivacyGuideStep.COOKIES,
          isAvailable: () => true,
        },
      ],
    ]);
  }

  private exitIfNecessary(): boolean {
    if (!this.isPrivacyGuideAvailable) {
      Router.getInstance().navigateTo(routes.PRIVACY);
      return true;
    }
    return false;
  }

  /** Handler for when the sync state is pushed from the browser. */
  private onSyncStatusChanged_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
    this.navigateForwardIfCurrentCardNoLongerAvailable();
  }

  /** Update the privacy guide state based on changed prefs. */
  private onPrefsChanged_() {
    // If this change resulted in the user no longer being in one of the
    // available states for the given card, we need to skip it.
    this.navigateForwardIfCurrentCardNoLongerAvailable();
  }

  private navigateForwardIfCurrentCardNoLongerAvailable() {
    if (!this.privacyGuideStep_) {
      // Not initialized.
      return;
    }
    if (!this.privacyGuideStepToComponentsMap_.get(this.privacyGuideStep_)!
             .isAvailable()) {
      // This card is currently shown but is no longer available. Navigate to
      // the next card in the flow.
      this.navigateForward_();
    }
  }

  /** Sets the privacy guide step from the URL parameter. */
  private async updateStateFromQueryParameters_() {
    assert(Router.getInstance().getCurrentRoute() === routes.PRIVACY_GUIDE);

    // Tasks in the privacy guide UI and in multiple fragments rely on prefs
    // being loaded. Instead of individually delaying those tasks, await prefs
    // once when a navigation to the privacy guide happens.
    await CrSettingsPrefs.initialized;
    // Set the pref that the user has viewed the Privacy guide.
    this.setPrefValue('privacy_guide.viewed', true);

    const step = Router.getInstance().getQueryParameters().get('step') as
        PrivacyGuideStep;

    if (this.privacyGuideStep_ && step === this.privacyGuideStep_) {
      // This is the currently shown step. No need to navigate.
      return;
    }

    if (Object.values(PrivacyGuideStep).includes(step)) {
      this.navigateToCard_(step, false, true);
    } else {
      // If no step has been specified, then navigate to the welcome step.
      this.navigateToCard_(PrivacyGuideStep.WELCOME, false, true);
    }
  }

  private onNextButtonClick_() {
    this.navigateForward_();
  }

  private recordEligibleSteps_(): void {
    for (const key in PrivacyGuideStep) {
      const step = PrivacyGuideStep[key as keyof typeof PrivacyGuideStep];
      if (!loadTimeData.getBoolean('enablePrivacyGuide3') &&
          step === PrivacyGuideStep.SEARCH_SUGGESTIONS) {
        // TODO(crbug.com/1215630): Search suggestion metrics should only be
        // recorded if the feature is enabled.
        continue;
      }

      if (step === PrivacyGuideStep.PRELOAD) {
        // TODO(crbug.com/1215630): No metrics are recorded for preload card
        // now.
        continue;
      }

      if (step === PrivacyGuideStep.WELCOME) {
        // This card has no status since it is always eligible to be shown and
        // is always reached.
        continue;
      }

      const component = this.privacyGuideStepToComponentsMap_.get(step);
      assert(component);
      if (!component.isAvailable()) {
        continue;
      }
      this.metricsBrowserProxy_
          .recordPrivacyGuideStepsEligibleAndReachedHistogram(
              eligibilityToRecord(step));
    }
  }

  private navigateForward_() {
    const components =
        this.privacyGuideStepToComponentsMap_.get(this.privacyGuideStep_)!;
    if (components.onForwardNavigation) {
      components.onForwardNavigation();
    }
    if (components.nextStep) {
      this.navigateToCard_(components.nextStep, false, false);
    }
  }

  private onBackButtonClick_() {
    this.navigateBackward_();
  }

  private navigateBackward_() {
    const components =
        this.privacyGuideStepToComponentsMap_.get(this.privacyGuideStep_)!;
    if (components.onBackwardNavigation) {
      components.onBackwardNavigation();
    }
    if (components.previousStep) {
      this.navigateToCard_(components.previousStep, true, false);
    }
  }

  private navigateToCard_(
      step: PrivacyGuideStep, isBackwardNavigation: boolean,
      isFirstNavigation: boolean) {
    assert(step !== this.privacyGuideStep_);
    this.privacyGuideStep_ = step;

    // When text direction is LTR, the pages are laid out left to right, so
    // when the user moves to the next page, the next page animates from right
    // to left. If the user goes to the previous page, the previous page
    // animates from left to right. If the text direction is RTL, this is
    // reversed.
    const animateFromLeftToRight = isBackwardNavigation ===
        (loadTimeData.getString('textdirection') === 'ltr');
    this.translateMultiplier_ = animateFromLeftToRight ? -1 : 1;

    if (!this.privacyGuideStepToComponentsMap_.get(step)!.isAvailable()) {
      // This card is currently not available. Navigate to the next one, or
      // the previous one if this was a back navigation.
      if (isBackwardNavigation) {
        this.navigateBackward_();
      } else {
        this.navigateForward_();
      }
    } else {
      if (this.animationsEnabled_) {
        this.$.viewManager.switchView(
            this.privacyGuideStep_, 'no-animation', 'fade-out');
      } else {
        this.$.viewManager.switchView(
            this.privacyGuideStep_, 'no-animation', 'no-animation');
      }
      Router.getInstance().updateRouteParams(
          new URLSearchParams('step=' + step));

      if (isFirstNavigation) {
        return;
      }

      // On navigations within privacy guide, put the focus on the newly shown
      // fragment.
      const elementToFocus = this.shadowRoot!.querySelector<HTMLElement>(
          '#' + this.privacyGuideStep_);
      assert(elementToFocus);
      afterNextRender(this, () => elementToFocus.focus());
    }
  }

  private computeBackButtonClass_(): string {
    if (!this.privacyGuideStep_) {
      // Not initialized.
      return '';
    }
    const components =
        this.privacyGuideStepToComponentsMap_.get(this.privacyGuideStep_)!;
    return (components.previousStep === undefined ? 'visibility-hidden' : '');
  }

  // TODO(rainhard): This is made public only because it is accessed by tests.
  // Should change tests so that this method can be made private again.
  computeStepIndicatorModel(): StepIndicatorModel {
    let stepCount = 0;
    let activeIndex = 0;
    const steps = loadTimeData.getBoolean('enablePrivacyGuide3') ?
        PrivacyGuideStep :
        PrivacyGuideStepPg3Off;
    for (const step of Object.values(steps)) {
      if (step === PrivacyGuideStep.WELCOME ||
          step === PrivacyGuideStep.COMPLETION) {
        // This card has no step in the step indicator.
        continue;
      }

      if (this.privacyGuideStepToComponentsMap_.get(step)!.isAvailable()) {
        if (step === this.privacyGuideStep_) {
          activeIndex = stepCount;
        }
        ++stepCount;
      }
    }
    return {
      active: activeIndex,
      total: stepCount,
    };
  }

  private isSyncOn_(): boolean {
    assert(this.syncStatus_);
    return !!this.syncStatus_.signedIn && !this.syncStatus_.hasError;
  }

  private shouldShowCookiesCard_(): boolean {
    if (!this.prefs) {
      // Prefs are not available yet. Show the card until they become available.
      return true;
    }
    const currentCookieSetting =
        this.getPref('generated.cookie_primary_setting').value;
    return currentCookieSetting === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
        currentCookieSetting ===
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
  }

  private shouldShowSafeBrowsingCard_(): boolean {
    if (!this.prefs) {
      // Prefs are not available yet. Show the card until they become available.
      return true;
    }
    const currentSafeBrowsingSetting =
        this.getPref('generated.safe_browsing').value;
    return currentSafeBrowsingSetting === SafeBrowsingSetting.ENHANCED ||
        currentSafeBrowsingSetting === SafeBrowsingSetting.STANDARD;
  }

  private shouldShowPreloadCard_(): boolean {
    if (!loadTimeData.getBoolean('enablePrivacyGuidePreload')) {
      // The preload card should only be available when the feature is on.
      return false;
    }
    if (!this.prefs) {
      // Prefs are not available yet. Show the card until they become available.
      return true;
    }
    const currentPreloadSettings =
        this.getPref<NetworkPredictionOptions>('net.network_prediction_options')
            .value;
    return currentPreloadSettings === NetworkPredictionOptions.STANDARD ||
        currentPreloadSettings === NetworkPredictionOptions.DISABLED;
  }

  private showAnySettingFragment_(): boolean {
    return this.privacyGuideStep_ !== PrivacyGuideStep.WELCOME &&
        this.privacyGuideStep_ !== PrivacyGuideStep.COMPLETION;
  }

  private onKeyDown_(event: KeyboardEvent) {
    const isLtr = loadTimeData.getString('textdirection') === 'ltr';
    switch (event.key) {
      case 'ArrowLeft':
        isLtr ? this.navigateBackward_() : this.navigateForward_();
        break;
      case 'ArrowRight':
        isLtr ? this.navigateForward_() : this.navigateBackward_();
        break;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-guide-page': SettingsPrivacyGuidePageElement;
  }
}

customElements.define(
    SettingsPrivacyGuidePageElement.is, SettingsPrivacyGuidePageElement);
