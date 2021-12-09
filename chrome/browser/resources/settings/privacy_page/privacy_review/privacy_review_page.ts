// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-review-page' is the settings page that helps users review
 * various privacy settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../../prefs/prefs.js';
import '../../settings_shared_css.js';
import './privacy_review_clear_on_exit_fragment.js';
import './privacy_review_completion_fragment.js';
import './privacy_review_cookies_fragment.js';
import './privacy_review_history_sync_fragment.js';
import './privacy_review_msbb_fragment.js';
import './privacy_review_safe_browsing_fragment.js';
import './privacy_review_welcome_fragment.js';
import './step_indicator.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../../hats_browser_proxy.js';
import {SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '../../people_page/sync_browser_proxy.js';
import {PrefsMixin, PrefsMixinInterface} from '../../prefs/prefs_mixin.js';
import {SafeBrowsingSetting} from '../../privacy_page/security_page.js';
import {routes} from '../../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../../router.js';
import {CookiePrimarySetting} from '../../site_settings/site_settings_prefs_browser_proxy.js';

import {PrivacyReviewStep} from './constants.js';
import {StepIndicatorModel} from './step_indicator.js';

interface PrivacyReviewStepComponents {
  onForwardNavigation(): void;
  onBackNavigation?(): void;
  isAvailable(): boolean;
}

const PrivacyReviewBase = RouteObserverMixin(WebUIListenerMixin(
                              I18nMixin(PrefsMixin(PolymerElement)))) as {
  new (): PolymerElement & I18nMixinInterface & WebUIListenerMixinInterface &
  RouteObserverMixinInterface & PrefsMixinInterface
};

export class SettingsPrivacyReviewPageElement extends PrivacyReviewBase {
  static get is() {
    return 'settings-privacy-review-page';
  }

  static get template() {
    return html`{__html_template__}`;
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
       * Valid privacy review states.
       */
      privacyReviewStepEnum_: {
        type: Object,
        value: PrivacyReviewStep,
      },

      /**
       * The current step in the privacy review flow.
       */
      privacyReviewStep_: {
        type: String,
        value: PrivacyReviewStep.WELCOME,
      },

      /**
       * Used by the 'step-indicator' element to display its dots.
       */
      stepIndicatorModel_: {
        type: Object,
        computed:
            'computeStepIndicatorModel(privacyReviewStep_, prefs.generated.cookie_primary_setting, prefs.generated.safe_browsing)',
      },
    };
  }

  static get observers() {
    return [
      `onPrefsChanged_(prefs.generated.cookie_primary_setting, prefs.generated.safe_browsing)`
    ];
  }

  private privacyReviewStep_: PrivacyReviewStep;
  private stepIndicatorModel_: StepIndicatorModel;
  private privacyReviewStepToComponentsMap_:
      Map<PrivacyReviewStep, PrivacyReviewStepComponents>;
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private syncStatus_: SyncStatus;

  constructor() {
    super();

    this.privacyReviewStepToComponentsMap_ =
        this.computePrivacyReviewStepToComponentsMap_();
  }

  ready() {
    super.ready();

    this.addWebUIListener(
        'sync-status-changed',
        (syncStatus: SyncStatus) => this.onSyncStatusChange_(syncStatus));
    this.syncBrowserProxy_.getSyncStatus().then(
        (syncStatus: SyncStatus) => this.onSyncStatusChange_(syncStatus));
  }

  /** RouteObserverBehavior */
  currentRouteChanged(newRoute: Route) {
    if (newRoute === routes.PRIVACY_REVIEW) {
      // Set the pref that the user has viewed the Privacy guide.
      this.setPrefValue('privacy_guide.viewed', true);

      this.updateStateFromQueryParameters_();
    }
  }

  /**
   * @return the map of privacy review steps to their components.
   */
  private computePrivacyReviewStepToComponentsMap_():
      Map<PrivacyReviewStep, PrivacyReviewStepComponents> {
    return new Map([
      [
        PrivacyReviewStep.WELCOME,
        {
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.MSBB);
          },
          isAvailable: () => this.shouldShowWelcomeCard_(),
        },
      ],
      [
        PrivacyReviewStep.COMPLETION,
        {
          onForwardNavigation: () => {
            Router.getInstance().navigateToPreviousRoute();
          },
          onBackNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.COOKIES, true);
          },
          isAvailable: () => true,
        },
      ],
      [
        PrivacyReviewStep.MSBB,
        {
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.CLEAR_ON_EXIT);
          },
          isAvailable: () => true,
        },
      ],
      [
        PrivacyReviewStep.CLEAR_ON_EXIT,
        {
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.HISTORY_SYNC);
          },
          onBackNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.MSBB, true);
          },
          // TODO(crbug/1215630): Enable the CoE step when it's ready.
          isAvailable: () => false,
        },
      ],
      [
        PrivacyReviewStep.HISTORY_SYNC,
        {
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.SAFE_BROWSING);
          },
          onBackNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.CLEAR_ON_EXIT, true);
          },
          isAvailable: () => this.isSyncOn_(),
        },
      ],
      [
        PrivacyReviewStep.SAFE_BROWSING,
        {
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.COOKIES);
          },
          onBackNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.HISTORY_SYNC, true);
          },
          isAvailable: () => this.shouldShowSafeBrowsingCard_(),
        },
      ],
      [
        PrivacyReviewStep.COOKIES,
        {
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.COMPLETION);
            HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
                TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE);
          },
          onBackNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.SAFE_BROWSING, true);
          },
          isAvailable: () => this.shouldShowCookiesCard_(),
        },
      ],
    ]);
  }

  /** Handler for when the sync state is pushed from the browser. */
  private onSyncStatusChange_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
    this.navigateToNextCardIfCurrentCardNoLongerAvailable();
  }

  /** Update the privacy review state based on changed prefs. */
  private onPrefsChanged_() {
    // If this change resulted in the user no longer being in one of the
    // available states for the given card, we need to skip it.
    this.navigateToNextCardIfCurrentCardNoLongerAvailable();
  }

  private navigateToNextCardIfCurrentCardNoLongerAvailable() {
    if (!this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
             .isAvailable()) {
      // This card is currently shown but is no longer available. Navigate to
      // the next card in the flow.
      this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
          .onForwardNavigation();
    }
  }

  /** Sets the privacy review step from the URL parameter. */
  private updateStateFromQueryParameters_() {
    assert(Router.getInstance().getCurrentRoute() === routes.PRIVACY_REVIEW);
    const step = Router.getInstance().getQueryParameters().get('step');
    // TODO(crbug/1215630): If the parameter is welcome but the user has opted
    // to skip the welcome card in a previous flow, then navigate to the first
    // settings card instead
    if (Object.values(PrivacyReviewStep).includes(step as PrivacyReviewStep)) {
      this.privacyReviewStep_ = step as PrivacyReviewStep;
    } else {
      // If no step has been specified, then navigate to the welcome step.
      this.navigateToCard_(PrivacyReviewStep.WELCOME);
    }
  }

  private navigateToCard_(
      step: PrivacyReviewStep, isBackwardNavigation?: boolean) {
    const nextState = this.privacyReviewStepToComponentsMap_.get(step)!;
    if (!nextState.isAvailable()) {
      // This card is currently not available. Navigate to the next one, or
      // the previous one if this was a back navigation.
      if (isBackwardNavigation) {
        if (nextState.onBackNavigation) {
          nextState.onBackNavigation();
        }
      } else {
        nextState.onForwardNavigation();
      }
    } else {
      Router.getInstance().updateRouteParams(
          new URLSearchParams('step=' + step));
      // TODO(crbug/1215630): Programmatically put the focus to the
      // corresponding element.
    }
  }

  private onNextButtonClick_() {
    this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
        .onForwardNavigation();
  }

  private computeBackButtonClass_(): string {
    return (
        this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
                    .onBackNavigation === undefined ?
            'visibility-hidden' :
            '');
  }

  private onBackButtonClick_() {
    this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
        .onBackNavigation!();
  }

  // TODO(rainhard): This is made public only because it is accessed by tests.
  // Should change tests so that this method can be made private again.
  computeStepIndicatorModel(): StepIndicatorModel {
    let stepCount = 0;
    let activeIndex = 0;
    for (const step of Object.values(PrivacyReviewStep)) {
      if (step === PrivacyReviewStep.WELCOME ||
          step === PrivacyReviewStep.COMPLETION) {
        // This card has no step in the step indicator.
        continue;
      }
      if (this.privacyReviewStepToComponentsMap_.get(step)!.isAvailable()) {
        if (step === this.privacyReviewStep_) {
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
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        !this.syncStatus_.hasError;
  }

  private shouldShowWelcomeCard_(): boolean {
    return this.getPref('privacy_review.show_welcome_card').value;
  }

  private shouldShowCookiesCard_(): boolean {
    const currentCookieSetting =
        this.getPref('generated.cookie_primary_setting').value;
    return currentCookieSetting === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
        currentCookieSetting ===
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
  }

  private shouldShowSafeBrowsingCard_(): boolean {
    const currentSafeBrowsingSetting =
        this.getPref('generated.safe_browsing').value;
    return currentSafeBrowsingSetting === SafeBrowsingSetting.ENHANCED ||
        currentSafeBrowsingSetting === SafeBrowsingSetting.STANDARD;
  }

  private showFragment_(step: PrivacyReviewStep): boolean {
    return this.privacyReviewStep_ === step;
  }

  private showAnySettingFragment_(): boolean {
    return this.privacyReviewStep_ !== PrivacyReviewStep.WELCOME &&
        this.privacyReviewStep_ !== PrivacyReviewStep.COMPLETION;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-review-page': SettingsPrivacyReviewPageElement;
  }
}

customElements.define(
    SettingsPrivacyReviewPageElement.is, SettingsPrivacyReviewPageElement);
