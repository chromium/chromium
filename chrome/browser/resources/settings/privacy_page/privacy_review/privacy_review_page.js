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
import './privacy_review_msbb_fragment.js';
import './step_indicator.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../../router.js';

/**
 * Steps in the privacy review flow. The page updates from those steps to show
 * the corresponding page content.
 * @enum {string}
 */
const PrivacyReviewStep = {
  WELCOME: 'welcome',
  MSBB: 'msbb',
  CLEAR_ON_EXIT: 'clearOnExit',
  COMPLETION: 'completion',
};

/**
 * TODO(crbug/1215630): This should be computed from the number of steps the
 * user will actually see, but not all steps are implemented yet.
 * @type {number}
 */
const REVIEW_STEPS = 5;

/**
 * @typedef {{
 *   headerString: (string|undefined),
 *   onNextButtonClick: function(),
 *   onBackButtonClick: (function()|undefined),
 * }}
 */
let PrivacyReviewStepComponents;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverMixinInterface}
 */
const PrivacyReviewBase =
    mixinBehaviors([I18nBehavior], RouteObserverMixin(PolymerElement));

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
       * The current step in the privacy review flow.
       * @private {PrivacyReviewStep}
       */
      privacyReviewStep_: {
        type: String,
        value: PrivacyReviewStep.WELCOME,
      },

      /**
       * Used by the 'step-indicator' element to display its dots.
       * @private {!Object<number, number>}
       */
      stepIndicatorModel_: {
        type: Object,
        computed: 'computeStepIndicatorModel_(privacyReviewStep_)',
      },
    };
  }

  constructor() {
    super();

    /** @private {Map<PrivacyReviewStep, PrivacyReviewStepComponents>} */
    this.privacyReviewStepToComponentsMap_ =
        this.computePrivacyReviewStepToComponentsMap_();
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route=} opt_oldRoute
   */
  currentRouteChanged(newRoute, opt_oldRoute) {
    if (newRoute === routes.PRIVACY_REVIEW) {
      this.updateStateFromQueryParameters_();
    }
  }

  /**
   * Returns the map of privacy review steps to their components.
   * @return {Map<PrivacyReviewStep, PrivacyReviewStepComponents>}
   * @private
   */
  computePrivacyReviewStepToComponentsMap_() {
    // This allows states to directly call page actions.
    const page = this;

    return new Map([
      [
        PrivacyReviewStep.WELCOME,
        {
          onNextButtonClick: function() {
            page.navigateToCard_(PrivacyReviewStep.MSBB);
          },
        },
      ],
      [
        PrivacyReviewStep.COMPLETION,
        {
          onNextButtonClick: function() {
            Router.getInstance().navigateToPreviousRoute();
          },
        },
      ],
      [
        PrivacyReviewStep.MSBB,
        {
          headerString: page.i18n('privacyReviewMsbbCardHeader'),
          onNextButtonClick: function() {
            page.navigateToCard_(PrivacyReviewStep.CLEAR_ON_EXIT);
          },
        },
      ],
      [
        PrivacyReviewStep.CLEAR_ON_EXIT,
        {
          headerString: page.i18n('privacyReviewClearOnExitCardHeader'),
          onNextButtonClick: function() {
            page.navigateToCard_(PrivacyReviewStep.COMPLETION);
          },
          onBackButtonClick: function() {
            page.navigateToCard_(PrivacyReviewStep.MSBB);
          },
        },
      ],
    ]);
  }

  /**
   * Sets the privacy review step from the URL parameter.
   * @private
   */
  updateStateFromQueryParameters_() {
    assert(Router.getInstance().getCurrentRoute() === routes.PRIVACY_REVIEW);
    const step = Router.getInstance().getQueryParameters().get('step');
    // TODO(crbug/1215630): If the parameter is welcome but the user has opted
    // to skip the welcome card in a previous flow, then navigate to the first
    // settings card instead
    if (Object.values(PrivacyReviewStep).includes(step)) {
      this.privacyReviewStep_ = /** @type !PrivacyReviewStep */ (step);
    } else {
      // If no step has been specified, then navigate to the welcome step.
      this.navigateToCard_(PrivacyReviewStep.WELCOME);
    }
  }

  /**
   * @private
   * @param {!PrivacyReviewStep} step
   */
  navigateToCard_(step) {
    Router.getInstance().updateRouteParams(new URLSearchParams('step=' + step));
    // TODO(crbug/1215630): Programmatically put the focus to the corresponding
    // element.
  }

  /** @private */
  onNextButtonClick_() {
    this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)
        .onNextButtonClick();
  }

  /**
   * @private
   * @return {string}
   */
  computeBackButtonClass_() {
    return 'cr-button' +
        (this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)
                     .onBackButtonClick === undefined ?
             ' visibility-hidden' :
             '');
  }

  /** @private */
  onBackButtonClick_() {
    this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)
        .onBackButtonClick();
  }

  /**
   * @private
   * @return {number}
   */
  computeActiveStepIndex_() {
    switch (this.privacyReviewStep_) {
      case PrivacyReviewStep.MSBB:
        return 0;
      case PrivacyReviewStep.CLEAR_ON_EXIT:
        return 1;
      default:
        // Welcome or completion cards do not show the step indicator, but since
        // the HTML element is still present it's computed anyway.
        return 0;
    }
  }

  /**
   * @private
   * @return {!Object<number, number>}
   */
  computeStepIndicatorModel_() {
    return {
      active: this.computeActiveStepIndex_(),
      total: REVIEW_STEPS,
    };
  }

  /**
   * @private
   * @return {string|undefined}
   */
  computeHeaderString_() {
    return this.privacyReviewStepToComponentsMap_.get(this.privacyReviewStep_)
        .headerString;
  }

  /**
   * @private
   * @return {boolean}
   */
  showHeader_() {
    return !!this.computeHeaderString_();
  }

  /**
   * @private
   * @return {boolean}
   */
  showWelcomeFragment_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.WELCOME;
  }

  /**
   * @private
   * @return {boolean}
   */
  showCompletionFragment_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.COMPLETION;
  }

  /**
   * @private
   * @return {boolean}
   */
  showAnySettingFragment_() {
    return !this.showWelcomeFragment_() && !this.showCompletionFragment_();
  }

  /**
   * @private
   * @return {boolean}
   */
  showMsbbFragment_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.MSBB;
  }

  /**
   * @private
   * @return {boolean}
   */
  showClearOnExitFragment_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.CLEAR_ON_EXIT;
  }
}

customElements.define(
    SettingsPrivacyReviewPageElement.is, SettingsPrivacyReviewPageElement);
