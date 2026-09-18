// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-guide-page' is the settings page that helps users guide
 * various privacy settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './privacy_guide_completion_fragment.js';
import './privacy_guide_cookies_fragment.js';
import './privacy_guide_history_sync_fragment.js';
import './privacy_guide_msbb_fragment.js';
import './privacy_guide_safe_browsing_fragment.js';
import './privacy_guide_welcome_fragment.js';
import './step_indicator.js';

import type {SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyGuideStepsEligibleAndReached} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {RouteObserverMixinLit, Router} from '../../router.js';
import {ContentSetting} from '../../site_settings/constants.js';
import {SafeBrowsingSetting} from '../security/safe_browsing_types.js';

import {PrivacyGuideStep} from './constants.js';
import {PrivacyGuideAvailabilityMixinLit} from './privacy_guide_availability_mixin_lit.js';
import {getCss} from './privacy_guide_page.css.js';
import {getHtml} from './privacy_guide_page.html.js';
import type {StepIndicatorModel} from './step_indicator.js';

interface PrivacyGuideStepComponents {
  nextStep?: PrivacyGuideStep;
  recordForwardNavigationMetrics?(): void;
  previousStep?: PrivacyGuideStep;
  recordBackwardNavigationMetrics?(): void;
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

const PrivacyGuideBase = RouteObserverMixinLit(
    PrivacyGuideAvailabilityMixinLit(WebUiListenerMixinLit(
        PrefServiceObserverMixinLit(I18nMixinLit(CrLitElement)))));

export class SettingsPrivacyGuidePageElement extends PrivacyGuideBase {
  static get is() {
    return 'settings-privacy-guide-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      privacyGuideStep_: {type: String},
      stepIndicatorModel_: {type: Object},
      syncStatus_: {type: Object},
      translateMultiplier_: {type: Number},
    };
  }

  protected accessor privacyGuideStep_: PrivacyGuideStep;
  protected accessor stepIndicatorModel_: StepIndicatorModel;
  private accessor syncStatus_: SyncStatus;
  protected accessor translateMultiplier_: number = 1;

  private animationsEnabled_: boolean = true;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private privacyGuideStepToComponentsMap_:
      Map<PrivacyGuideStep, PrivacyGuideStepComponents>;

  constructor() {
    super();

    this.privacyGuideStepToComponentsMap_ =
        this.computePrivacyGuideStepToComponentsMap_();
  }

  override connectedCallback() {
    super.connectedCallback();

    const prefsToObserve = [
      'generated.cookie_default_content_setting',
      'generated.safe_browsing',
      'generated.third_party_cookie_blocking_setting',
      'net.network_prediction_options',
    ];
    for (const pref of prefsToObserve) {
      this.addPrefObserver(pref, () => this.onPrefsChanged_());
    }

    this.addWebUiListener(
        'sync-status-changed',
        (syncStatus: SyncStatus) => this.onSyncStatusChanged_(syncStatus));
    this.syncBrowserProxy_.getSyncStatus().then(
        (syncStatus: SyncStatus) => this.onSyncStatusChanged_(syncStatus));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('privacyGuideStep_')) {
      this.stepIndicatorModel_ = this.computeStepIndicatorModel();
    }
    if (changedPrivateProperties.has('isPrivacyGuideAvailable')) {
      this.exitIfNecessary();
    }
  }

  disableAnimationsForTesting() {
    this.animationsEnabled_ = false;
  }

  /** RouteObserverMixinLit */
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
    return new Map([
      [
        PrivacyGuideStep.WELCOME,
        {
          nextStep: PrivacyGuideStep.MSBB,
          isAvailable: () => true,
          recordForwardNavigationMetrics: () => {
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
          recordForwardNavigationMetrics: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.MSBB_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickMSBB');
          },
          recordBackwardNavigationMetrics: () => {
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
          recordForwardNavigationMetrics: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickHistorySync');
          },
          recordBackwardNavigationMetrics: () => {
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.BackClickHistorySync');
          },
          // Allow the history sync card to be shown while `syncStatus_` is
          // unavailable. Otherwise we would skip it in
          // `navigateForwardIfCurrentCardNoLongerAvailable` before
          // `onSyncStatusChanged_` is called asynchronously.
          isAvailable: () =>
              !this.syncStatus_ || this.shouldShowHistorySyncCard_(),
        },
      ],
      [
        PrivacyGuideStep.SAFE_BROWSING,
        {
          nextStep: PrivacyGuideStep.COOKIES,
          previousStep: PrivacyGuideStep.HISTORY_SYNC,
          recordForwardNavigationMetrics: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickSafeBrowsing');
          },
          recordBackwardNavigationMetrics: () => {
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
          recordForwardNavigationMetrics: () => {
            this.metricsBrowserProxy_.recordPrivacyGuideNextNavigationHistogram(
                PrivacyGuideInteractions.COOKIES_NEXT_BUTTON);
            this.metricsBrowserProxy_.recordAction(
                'Settings.PrivacyGuide.NextClickCookies');
          },
          recordBackwardNavigationMetrics: () => {
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
          recordBackwardNavigationMetrics: () => {
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
    this.stepIndicatorModel_ = this.computeStepIndicatorModel();
    this.navigateForwardIfCurrentCardNoLongerAvailable();
  }

  /** Update the privacy guide state based on changed prefs. */
  private onPrefsChanged_() {
    this.stepIndicatorModel_ = this.computeStepIndicatorModel();
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
    await PrefService.getInstance().whenInitialized();
    // Set the pref that the user has viewed the Privacy guide.
    PrefService.getInstance().setPrefValue('privacy_guide.viewed', true);

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

  protected onNextButtonClick_() {
    this.navigateForward_();
  }

  protected onStartButtonClick_() {
    this.navigateForward_();
  }

  private recordEligibleSteps_(): void {
    for (const key in PrivacyGuideStep) {
      const step = PrivacyGuideStep[key as keyof typeof PrivacyGuideStep];
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
    if (components.isAvailable() && components.recordForwardNavigationMetrics) {
      components.recordForwardNavigationMetrics();
    }
    if (components.nextStep) {
      this.navigateToCard_(components.nextStep, false, false);
    }
  }

  protected onBackButtonClick_() {
    this.navigateBackward_();
  }

  private navigateBackward_() {
    const components =
        this.privacyGuideStepToComponentsMap_.get(this.privacyGuideStep_)!;
    if (components.isAvailable() &&
        components.recordBackwardNavigationMetrics) {
      components.recordBackwardNavigationMetrics();
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
      const elementToFocus = this.shadowRoot.querySelector<HTMLElement>(
          '#' + this.privacyGuideStep_);
      assert(elementToFocus);
      elementToFocus.focus();
    }
  }

  protected computeBackButtonClass_(): string {
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
    for (const step of Object.values(PrivacyGuideStep)) {
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

  private shouldShowHistorySyncCard_(): boolean {
    assert(this.syncStatus_);
    if (this.syncStatus_.hasError) {
      return false;
    }
    return this.syncStatus_.signedInState === SignedInState.SYNCING ||
        (loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') &&
         this.syncStatus_.signedInState === SignedInState.SIGNED_IN);
  }

  private shouldShowCookiesCard_(): boolean {
    // Don't show the 3PC card if the user has chosen to block 1PCs.
    return PrefService.getInstance()
               .getPref<ContentSetting>(
                   'generated.cookie_default_content_setting')
               .value !== ContentSetting.BLOCK;
  }

  private shouldShowSafeBrowsingCard_(): boolean {
    const currentSafeBrowsingSetting =
        PrefService.getInstance()
            .getPref<SafeBrowsingSetting>('generated.safe_browsing')
            .value;
    return currentSafeBrowsingSetting === SafeBrowsingSetting.ENHANCED ||
        currentSafeBrowsingSetting === SafeBrowsingSetting.STANDARD;
  }

  protected showAnySettingFragment_(): boolean {
    return this.privacyGuideStep_ !== PrivacyGuideStep.WELCOME &&
        this.privacyGuideStep_ !== PrivacyGuideStep.COMPLETION;
  }

  protected onKeydown_(event: KeyboardEvent) {
    const isLtr = loadTimeData.getString('textdirection') === 'ltr';
    switch (event.key) {
      case 'ArrowLeft':
        isLtr ? this.navigateBackward_() : this.navigateForward_();
        break;
      case 'ArrowRight':
        isLtr ? this.navigateForward_() : this.navigateBackward_();
        break;
      default:
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
