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
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './privacy_review_clear_on_exit_fragment.js';
import './privacy_review_completion_fragment.js';
import './privacy_review_cookies_fragment.js';
import './privacy_review_history_sync_fragment.js';
import './privacy_review_msbb_fragment.js';
import './privacy_review_safe_browsing_fragment.js';
import './privacy_review_welcome_fragment.js';
import './step_indicator.js';

import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
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
  nextStep?: PrivacyReviewStep;
  onForwardNavigation?(): void;
  previousStep?: PrivacyReviewStep;
  isAvailable(): boolean;
}

export interface SettingsPrivacyReviewPageElement {
  $: {
    viewManager: CrViewManagerElement,
  },
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
       * The current step in the privacy review flow, or `undefined` if the flow
       * has not yet been initialized from query parameters.
       */
      privacyReviewStep_: {
        type: String,
        value: undefined,
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
  private animationsEnabled_: boolean = true;

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

  disableAnimationsForTesting() {
    this.animationsEnabled_ = false;
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
          nextStep: PrivacyReviewStep.MSBB,
          isAvailable: () => this.shouldShowWelcomeCard_(),
        },
      ],
      [
        PrivacyReviewStep.COMPLETION,
        {
          onForwardNavigation: () => {
            Router.getInstance().navigateToPreviousRoute();
          },
          previousStep: PrivacyReviewStep.COOKIES,
          isAvailable: () => true,
        },
      ],
      [
        PrivacyReviewStep.MSBB,
        {
          nextStep: PrivacyReviewStep.CLEAR_ON_EXIT,
          isAvailable: () => true,
        },
      ],
      [
        PrivacyReviewStep.CLEAR_ON_EXIT,
        {
          nextStep: PrivacyReviewStep.HISTORY_SYNC,
          previousStep: PrivacyReviewStep.MSBB,
          // TODO(crbug/1215630): Enable the CoE step when it's ready.
          isAvailable: () => false,
        },
      ],
      [
        PrivacyReviewStep.HISTORY_SYNC,
        {
          nextStep: PrivacyReviewStep.SAFE_BROWSING,
          previousStep: PrivacyReviewStep.CLEAR_ON_EXIT,
          // Allow the history sync card to be shown while `syncStatus_` is
          // unavailable. Otherwise we would skip it in
          // `navigateForwardIfCurrentCardNoLongerAvailable` before
          // `onSyncStatusChange_` is called asynchronously.
          isAvailable: () => !this.syncStatus_ || this.isSyncOn_(),
        },
      ],
      [
        PrivacyReviewStep.SAFE_BROWSING,
        {
          nextStep: PrivacyReviewStep.COOKIES,
          previousStep: PrivacyReviewStep.HISTORY_SYNC,
          isAvailable: () => this.shouldShowSafeBrowsingCard_(),
        },
      ],
      [
        PrivacyReviewStep.COOKIES,
        {
          nextStep: PrivacyReviewStep.COMPLETION,
          onForwardNavigation: () => {
            HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
                TrustSafetyInteraction.COMPLETED_PRIVACY_GUIDE);
          },
          previousStep: PrivacyReviewStep.SAFE_BROWSING,
          isAvailable: () => this.shouldShowCookiesCard_(),
        },
      ],
    ]);
  }

  /** Handler for when the sync state is pushed from the browser. */
  private onSyncStatusChange_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
    this.navigateForwardIfCurrentCardNoLongerAvailable();
  }

  /** Update the privacy review state based on changed prefs. */
  private onPrefsChanged_() {
    // If this change resulted in the user no longer being in one of the
    // available states for the given card, we need to skip it.
    this.navigateForwardIfCurrentCardNoLongerAvailable();
  }

  private navigateForwardIfCurrentCardNoLongerAvailable() {
    if (!this.privacyReviewStep_) {
      // Not initialized.
      return;
    }
    if (!this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
             .isAvailable()) {
      // This card is currently shown but is no longer available. Navigate to
      // the next card in the flow.
      this.navigateForward_(true);
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
      this.navigateToCard_(step as PrivacyReviewStep, false, true);
    } else {
      // If no step has been specified, then navigate to the welcome step.
      this.navigateToCard_(PrivacyReviewStep.WELCOME, false, false);
    }
  }

  private onNextButtonClick_() {
    this.navigateForward_(true);
  }

  private navigateForward_(playAnimation: boolean) {
    const components =
        this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!;
    assert(components.onForwardNavigation || components.nextStep);
    if (components.onForwardNavigation) {
      components.onForwardNavigation();
    }
    if (components.nextStep) {
      this.navigateToCard_(components.nextStep, false, playAnimation);
    }
  }

  private onBackButtonClick_() {
    this.navigateBackward_(true);
  }

  private navigateBackward_(playAnimation: boolean) {
    this.navigateToCard_(
        this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
            .previousStep!,
        true, playAnimation);
  }

  private navigateToCard_(
      step: PrivacyReviewStep, isBackwardNavigation: boolean,
      playAnimation: boolean) {
    if (step === this.privacyReviewStep_) {
      return;
    }
    this.privacyReviewStep_ = step;
    if (!this.privacyReviewStepToComponentsMap_.get(step)!.isAvailable()) {
      // This card is currently not available. Navigate to the next one, or
      // the previous one if this was a back navigation.
      if (isBackwardNavigation) {
        this.navigateBackward_(playAnimation);
      } else {
        this.navigateForward_(playAnimation);
      }
    } else {
      if (this.animationsEnabled_ && playAnimation) {
        this.$.viewManager.switchView(this.privacyReviewStep_);
      } else {
        this.$.viewManager.switchView(
            this.privacyReviewStep_, 'no-animation', 'no-animation');
      }
      Router.getInstance().updateRouteParams(
          new URLSearchParams('step=' + step));
      // TODO(crbug/1215630): Programmatically put the focus to the
      // corresponding element.
    }
  }

  private computeBackButtonClass_(): string {
    if (!this.privacyReviewStep_) {
      // Not initialized.
      return '';
    }
    const components =
        this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!;
    return (components.previousStep === undefined ? 'visibility-hidden' : '');
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
    assert(this.syncStatus_);
    return !!this.syncStatus_.signedIn && !this.syncStatus_.hasError;
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
