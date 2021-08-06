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

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../../router.js';

import {PrivacyReviewMsbbFragmentElement} from './privacy_review_msbb_fragment.js';
/**
 * Steps in the privacy review flow. The page updates from those steps to show
 * the corresponding page content.
 * @enum {string}
 */
const PrivacyReviewStep = {
  WELCOME: 'welcome',
  MSBB: 'msbb',
  COMPLETION: 'completion',
};

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
    };
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
    Router.getInstance().navigateTo(
        routes.PRIVACY_REVIEW,
        /* opt_dynamicParameters */ new URLSearchParams('step=' + step),
        /* opt_removeSearch */ false,
        /* opt_skipHistoryEntry */ true);
    // TODO(crbug/1215630): Programmatically put the focus to the corresponding
    // element.
  }

  /** @private */
  onNextButtonClick_() {
    switch (this.privacyReviewStep_) {
      case PrivacyReviewStep.WELCOME:
        this.navigateToCard_(PrivacyReviewStep.MSBB);
        break;
      case PrivacyReviewStep.COMPLETION:
        // TODO(crbug/1215630): Navigate to routes.PRIVACY and focus the
        // privacy review row.
        break;
      case PrivacyReviewStep.MSBB:
        this.navigateToCard_(PrivacyReviewStep.COMPLETION);
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * @private
   * @return string
   */
  computeHeaderString_() {
    switch (this.privacyReviewStep_) {
      case PrivacyReviewStep.MSBB:
        return this.i18n('privacyReviewMsbbCardHeader');
      default:
        return null;
    }
  }

  /**
   * @private
   * @return boolean
   */
  showHeader_() {
    return this.computeHeaderString_() != null;
  }

  /**
   * @private
   * @return string
   */
  computeNextButtonLabel_() {
    switch (this.privacyReviewStep_) {
      case PrivacyReviewStep.WELCOME:
        return this.i18n('privacyReviewWelcomeCardStartButton');
      case PrivacyReviewStep.COMPLETION:
        return this.i18n('privacyReviewCompletionCardLeaveButton');
      case PrivacyReviewStep.MSBB:
        return this.i18n('privacyReviewNextButton');
      default:
        return '';
    }
  }

  /**
   * @private
   * @return string
   */
  computeFooterClass_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.WELCOME ? null : 'hr';
  }

  /**
   * @private
   * @return boolean
   */
  showWelcomeFragment_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.WELCOME;
  }

  /**
   * @private
   * @return boolean
   */
  showCompletionFragment_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.COMPLETION;
  }

  /**
   * @private
   * @return boolean
   */
  showMsbbFragment_() {
    return this.privacyReviewStep_ === PrivacyReviewStep.MSBB;
  }
}

customElements.define(
    SettingsPrivacyReviewPageElement.is, SettingsPrivacyReviewPageElement);
