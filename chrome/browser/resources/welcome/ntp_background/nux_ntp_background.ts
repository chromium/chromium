// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/cr.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../shared/animations.css.js';
import '../shared/chooser_shared.css.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateToNextStep, NavigationMixin} from '../navigation_mixin.js';
import {ModuleMetricsManager} from '../shared/module_metrics_proxy.js';
import type {StepIndicatorModel} from '../shared/nux_types.js';

import {NtpBackgroundMetricsProxyImpl} from './ntp_background_metrics_proxy.js';
import type {NtpBackgroundData, NtpBackgroundProxy} from './ntp_background_proxy.js';
import {NtpBackgroundProxyImpl} from './ntp_background_proxy.js';
import {getTemplate} from './nux_ntp_background.html.js';

const KEYBOARD_FOCUSED_CLASS = 'keyboard-focused';

export interface NuxNtpBackgroundElement {
  $: {
    backgroundPreview: HTMLElement,
    skipButton: HTMLElement,
  };
}

const NuxNtpBackgroundElementBase = I18nMixin(NavigationMixin(PolymerElement));

/** @polymer */
export class NuxNtpBackgroundElement extends NuxNtpBackgroundElementBase {
  static get is() {
    return 'nux-ntp-background';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      indicatorModel: Object,

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

  private backgrounds_: NtpBackgroundData[]|null = null;
  private finalized_: boolean = false;
  private imageIsLoading_: boolean = false;
  private metricsManager_: ModuleMetricsManager;
  private ntpBackgroundProxy_: NtpBackgroundProxy;
  private selectedBackground_: NtpBackgroundData|undefined;
  indicatorModel?: StepIndicatorModel;

  constructor() {
    super();

    this.ntpBackgroundProxy_ = NtpBackgroundProxyImpl.getInstance();
    this.metricsManager_ =
        new ModuleMetricsManager(NtpBackgroundMetricsProxyImpl.getInstance());
  }

  override onRouteEnter() {
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

  override onRouteExit() {
    if (this.imageIsLoading_) {
      this.ntpBackgroundProxy_.recordBackgroundImageNeverLoaded();
    }

    if (this.finalized_) {
      return;
    }
    this.metricsManager_.recordBrowserBackOrForward();
  }

  override onRouteUnload() {
    if (this.imageIsLoading_) {
      this.ntpBackgroundProxy_.recordBackgroundImageNeverLoaded();
    }

    if (this.finalized_) {
      return;
    }
    this.metricsManager_.recordNavigatedAway();
  }

  private hasValidSelectedBackground_(): boolean {
    return this.selectedBackground_!.id > -1;
  }

  private isSelectedBackground_(background: NtpBackgroundData) {
    return background === this.selectedBackground_;
  }

  private onSelectedBackgroundChange_() {
    const id = this.selectedBackground_!.id;
    if (id > -1) {
      this.imageIsLoading_ = true;
      const imageUrl = this.selectedBackground_!.imageUrl;
      this.ntpBackgroundProxy_.preloadImage(imageUrl).then(
          () => {
            if (this.selectedBackground_!.id === id) {
              this.imageIsLoading_ = false;
              this.$.backgroundPreview.classList.add('active');
              this.$.backgroundPreview.style.backgroundImage =
                  `url(${imageUrl})`;
            }
          },
          () => {
            this.ntpBackgroundProxy_.recordBackgroundImageFailedToLoad();
          });
    } else {
      this.$.backgroundPreview.classList.remove('active');
    }
  }

  private onBackgroundPreviewTransitionEnd_() {
    // Whenever the #backgroundPreview transitions to a non-active, hidden
    // state, remove the background image. This way, when the element
    // transitions back to active, the previous background is not displayed.
    if (!this.$.backgroundPreview.classList.contains('active')) {
      this.$.backgroundPreview.style.backgroundImage = '';
    }
  }

  private announceA11y_(text: string) {
    this.dispatchEvent(new CustomEvent(
        'iron-announce', {bubbles: true, composed: true, detail: {text}}));
  }

  private onBackgroundClick_(e: {model: {item: NtpBackgroundData}}) {
    this.selectedBackground_ = e.model.item;
    this.metricsManager_.recordClickedOption();
    this.announceA11y_(this.i18n(
        'ntpBackgroundPreviewUpdated', this.selectedBackground_.title));
  }

  private onBackgroundKeyUp_(e: KeyboardEvent) {
    if (e.key === 'ArrowRight' || e.key === 'ArrowDown') {
      this.changeFocus_(e.currentTarget!, 1);
    } else if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') {
      this.changeFocus_(e.currentTarget!, -1);
    } else {
      this.changeFocus_(e.currentTarget!, 0);
    }
  }

  private changeFocus_(element: EventTarget, direction: number) {
    if (isRTL()) {
      direction *= -1;
    }

    // Reverse direction if RTL.
    const buttons =
        this.shadowRoot!.querySelectorAll<HTMLButtonElement>('.option');
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

  private onBackgroundPointerDown_(e: Event) {
    (e.currentTarget as HTMLElement).classList.remove(KEYBOARD_FOCUSED_CLASS);
  }

  private onNextClicked_() {
    this.finalized_ = true;

    if (this.selectedBackground_ && this.selectedBackground_.id > -1) {
      this.ntpBackgroundProxy_.setBackground(this.selectedBackground_.id);
    } else {
      this.ntpBackgroundProxy_.clearBackground();
    }
    this.metricsManager_.recordGetStarted();
    navigateToNextStep();
  }

  private onSkipClicked_() {
    this.finalized_ = true;
    this.metricsManager_.recordNoThanks();
    navigateToNextStep();
    if (this.hasValidSelectedBackground_()) {
      this.announceA11y_(this.i18n('ntpBackgroundReset'));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'nux-ntp-background': NuxNtpBackgroundElement;
  }
}

customElements.define(NuxNtpBackgroundElement.is, NuxNtpBackgroundElement);
