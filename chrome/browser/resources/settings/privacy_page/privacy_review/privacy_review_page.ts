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
import './privacy_review_history_sync_fragment.js';
import './privacy_review_msbb_fragment.js';
import './privacy_review_welcome_fragment.js';
import './step_indicator.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '../../people_page/sync_browser_proxy.js';
import {routes} from '../../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../../router.js';

import {StepIndicatorModel} from './step_indicator.js';

/**
 * Steps in the privacy review flow in their order of appearance. The page
 * updates from those steps to show the corresponding page content.
 */
enum PrivacyReviewStep {
  WELCOME = 'welcome',
  MSBB = 'msbb',
  CLEAR_ON_EXIT = 'clearOnExit',
  HISTORY_SYNC = 'historySync',
  COMPLETION = 'completion',
}

interface PrivacyReviewStepComponents {
  headerString?: string;
  onForwardNavigation(): void;
  onBackNavigation?(): void;
  isAvailable(): boolean;
}

const PrivacyReviewBase =
    RouteObserverMixin(WebUIListenerMixin(I18nMixin(PolymerElement))) as {
      new (): PolymerElement & I18nMixinInterface &
      WebUIListenerMixinInterface & RouteObserverMixinInterface
    };

/** @polymer */
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
       * @private {PrivacyReviewStep}
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
        computed: 'computeStepIndicatorModel_(privacyReviewStep_)',
      },
    };
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
          isAvailable: () => true,
        },
      ],
      [
        PrivacyReviewStep.COMPLETION,
        {
          onForwardNavigation: () => {
            Router.getInstance().navigateToPreviousRoute();
          },
          isAvailable: () => true,
        },
      ],
      [
        PrivacyReviewStep.MSBB,
        {
          headerString: this.i18n('privacyReviewMsbbCardHeader'),
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.CLEAR_ON_EXIT);
          },
          isAvailable: () => true,
        },
      ],
      [
        PrivacyReviewStep.CLEAR_ON_EXIT,
        {
          headerString: this.i18n('privacyReviewClearOnExitCardHeader'),
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
          headerString: this.i18n('privacyReviewHistorySyncCardHeader'),
          onForwardNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.COMPLETION);
          },
          onBackNavigation: () => {
            this.navigateToCard_(PrivacyReviewStep.CLEAR_ON_EXIT, true);
          },
          isAvailable: () => this.isSyncOn_(),
        },
      ],
    ]);
  }

  /** Handler for when the sync state is pushed from the browser. */
  private onSyncStatusChange_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
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
    return 'cr-button' +
        (this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
                     .onBackNavigation === undefined ?
             ' visibility-hidden' :
             '');
  }

  private onBackButtonClick_() {
    this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
        .onBackNavigation!();
  }

  private computeStepIndicatorModel_(): StepIndicatorModel {
    let stepCount = 0;
    let activeIndex = 0;
    for (const step of Object.values(PrivacyReviewStep)) {
      if (step === PrivacyReviewStep.WELCOME) {
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

  private computeHeaderString_(): string|undefined {
    return this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)!
        .headerString;
  }

  private isSyncOn_(): boolean {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        !this.syncStatus_.hasError;
  }

  private showHeader_(): boolean {
    return !!this.computeHeaderString_();
  }

  private showFragment_(step: PrivacyReviewStep): boolean {
    return this.privacyReviewStep_ === step;
  }

  private showAnySettingFragment_(): boolean {
    return this.privacyReviewStep_ !== PrivacyReviewStep.WELCOME &&
        this.privacyReviewStep_ !== PrivacyReviewStep.COMPLETION;
  }
}

customElements.define(
    SettingsPrivacyReviewPageElement.is, SettingsPrivacyReviewPageElement);
