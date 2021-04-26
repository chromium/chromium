// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/cr.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../shared/animations_css.js';
import '../shared/chooser_shared_css.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateToNextStep, NavigationBehavior, NavigationBehaviorInterface, Routes} from '../navigation_behavior.js';
import {ModuleMetricsManager} from '../shared/module_metrics_proxy.js';
import {stepIndicatorModel} from '../shared/nux_types.js';

import {NtpBackgroundMetricsProxyImpl} from './ntp_background_metrics_proxy.js';
import {NtpBackgroundData, NtpBackgroundProxy, NtpBackgroundProxyImpl} from './ntp_background_proxy.js';

const KEYBOARD_FOCUSED_CLASS = 'keyboard-focused';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {NavigationBehaviorInterface}
 */
const NuxNtpBackgroundElementBase =
    mixinBehaviors([I18nBehavior, NavigationBehavior], PolymerElement);

/** @polymer */
export class NuxNtpBackgroundElement extends NuxNtpBackgroundElementBase {
  static get is() {
    return 'nux-ntp-background';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {stepIndicatorModel} */
      indicatorModel: Object,

      /** @private {?NtpBackgroundData} */
      selectedBackground_: {
        observer: 'onSelectedBackgroundChange_',
        type: Object,
      },

      subtitle: {
        type: String,
        value: loadTimeData.getString('ntpBackgroundDescription'),
      },
    };
  }

  constructor() {
    super();

    /** @private {?Array<!NtpBackgroundData>} */
    this.backgrounds_ = null;

    /** @private */
    this.finalized_ = false;

    /** @private {boolean} */
    this.imageIsLoading_ = false;

    /** @private {?ModuleMetricsManager} */
    this.metricsManager_ = null;

    /** @private {?NtpBackgroundProxy} */
    this.ntpBackgroundProxy_ = null;
  }

  /** @override */
  ready() {
    super.ready();

    this.ntpBackgroundProxy_ = NtpBackgroundProxyImpl.getInstance();
    this.metricsManager_ =
        new ModuleMetricsManager(NtpBackgroundMetricsProxyImpl.getInstance());
  }

  onRouteEnter() {
    this.finalized_ = false;
    const defaultBackground = {
      id: -1,
      imageUrl: '',
      thumbnailClass: '',
      title: this.i18n('ntpBackgroundDefault'),
    };
    if (!this.selectedBackground_) {
      this.selectedBackground_ = defaultBackground;
    }
    if (!this.backgrounds_) {
      this.ntpBackgroundProxy_.getBackgrounds().then((backgrounds) => {
        this.backgrounds_ = [
          defaultBackground,
          ...backgrounds,
        ];
      });
    }
    this.metricsManager_.recordPageInitialized();
  }

  onRouteExit() {
    if (this.imageIsLoading_) {
      this.ntpBackgroundProxy_.recordBackgroundImageNeverLoaded();
    }

    if (this.finalized_) {
      return;
    }
    this.metricsManager_.recordBrowserBackOrForward();
  }

  onRouteUnload() {
    if (this.imageIsLoading_) {
      this.ntpBackgroundProxy_.recordBackgroundImageNeverLoaded();
    }

    if (this.finalized_) {
      return;
    }
    this.metricsManager_.recordNavigatedAway();
  }

  /**
   * @return {boolean}
   * @private
   */
  hasValidSelectedBackground_() {
    return this.selectedBackground_.id > -1;
  }

  /**
   * @param {!NtpBackgroundData} background
   * @private
   */
  isSelectedBackground_(background) {
    return background === this.selectedBackground_;
  }

  /** @private */
  onSelectedBackgroundChange_() {
    const id = this.selectedBackground_.id;

    if (id > -1) {
      this.imageIsLoading_ = true;
      const imageUrl = this.selectedBackground_.imageUrl;
      const beforeLoadTime = window.performance.now();
      this.ntpBackgroundProxy_.preloadImage(imageUrl).then(
          () => {
            if (this.selectedBackground_.id === id) {
              this.imageIsLoading_ = false;
              this.$.backgroundPreview.classList.add('active');
              this.$.backgroundPreview.style.backgroundImage =
                  `url(${imageUrl})`;
            }

            this.ntpBackgroundProxy_.recordBackgroundImageLoadTime(
                Math.floor(performance.now() - beforeLoadTime));
          },
          () => {
            this.ntpBackgroundProxy_.recordBackgroundImageFailedToLoad();
          });
    } else {
      this.$.backgroundPreview.classList.remove('active');
    }
  }

  /** @private */
  onBackgroundPreviewTransitionEnd_() {
    // Whenever the #backgroundPreview transitions to a non-active, hidden
    // state, remove the background image. This way, when the element
    // transitions back to active, the previous background is not displayed.
    if (!this.$.backgroundPreview.classList.contains('active')) {
      this.$.backgroundPreview.style.backgroundImage = '';
    }
  }

  /**
   * @param {string} text
   * @private
   */
  announceA11y_(text) {
    this.dispatchEvent(new CustomEvent(
        'iron-announce', {bubbles: true, composed: true, detail: {text}}));
  }

  /**
   * @param {!{model: !{item: !NtpBackgroundData}}} e
   * @private
   */
  onBackgroundClick_(e) {
    this.selectedBackground_ = e.model.item;
    this.metricsManager_.recordClickedOption();
    this.announceA11y_(this.i18n(
        'ntpBackgroundPreviewUpdated', this.selectedBackground_.title));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onBackgroundKeyUp_(e) {
    if (e.key === 'ArrowRight' || e.key === 'ArrowDown') {
      this.changeFocus_(e.currentTarget, 1);
    } else if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') {
      this.changeFocus_(e.currentTarget, -1);
    } else {
      this.changeFocus_(e.currentTarget, 0);
    }
  }

  /**
   * @param {EventTarget} element
   * @param {number} direction
   * @private
   */
  changeFocus_(element, direction) {
    if (isRTL()) {
      direction *= -1;  // Reverse direction if RTL.
    }

    const buttons = this.root.querySelectorAll('.option');
    const targetIndex = Array.prototype.indexOf.call(buttons, element);

    const oldFocus = buttons[targetIndex];
    if (!oldFocus) {
      return;
    }

    const newFocus = buttons[targetIndex + direction];

    if (newFocus && direction) {
      newFocus.classList.add(KEYBOARD_FOCUSED_CLASS);
      oldFocus.classList.remove(KEYBOARD_FOCUSED_CLASS);
      newFocus.focus();
    } else {
      oldFocus.classList.add(KEYBOARD_FOCUSED_CLASS);
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onBackgroundPointerDown_(e) {
    e.currentTarget.classList.remove(KEYBOARD_FOCUSED_CLASS);
  }

  /** @private */
  onNextClicked_() {
    this.finalized_ = true;

    if (this.selectedBackground_ && this.selectedBackground_.id > -1) {
      this.ntpBackgroundProxy_.setBackground(this.selectedBackground_.id);
    } else {
      this.ntpBackgroundProxy_.clearBackground();
    }
    this.metricsManager_.recordGetStarted();
    navigateToNextStep();
  }

  /** @private */
  onSkipClicked_() {
    this.finalized_ = true;
    this.metricsManager_.recordNoThanks();
    navigateToNextStep();

    if (this.hasValidSelectedBackground_()) {
      this.announceA11y_(this.i18n('ntpBackgroundReset'));
    }
  }
}
customElements.define(NuxNtpBackgroundElement.is, NuxNtpBackgroundElement);
