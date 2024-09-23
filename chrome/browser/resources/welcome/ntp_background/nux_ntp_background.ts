// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/js/cr.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {NavigationMixin} from '../navigation_mixin.js';
import {navigateToNextStep} from '../router.js';
import {ModuleMetricsManager} from '../shared/module_metrics_proxy.js';
import type {StepIndicatorModel} from '../shared/nux_types.js';

import {NtpBackgroundMetricsProxyImpl} from './ntp_background_metrics_proxy.js';
import type {NtpBackgroundData, NtpBackgroundProxy} from './ntp_background_proxy.js';
import {NtpBackgroundProxyImpl} from './ntp_background_proxy.js';
import {getCss} from './nux_ntp_background.css.js';
import {getHtml} from './nux_ntp_background.html.js';

const KEYBOARD_FOCUSED_CLASS = 'keyboard-focused';

export interface NuxNtpBackgroundElement {
  $: {
    backgroundPreview: HTMLElement,
    skipButton: HTMLElement,
  };
}

const NuxNtpBackgroundElementBase = I18nMixinLit(NavigationMixin(CrLitElement));

export class NuxNtpBackgroundElement extends NuxNtpBackgroundElementBase {
  static get is() {
    return 'nux-ntp-background';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      backgrounds_: {type: Array},
      selectedBackground_: {type: Object},
      indicatorModel: {type: Object},
    };
  }

  protected backgrounds_: NtpBackgroundData[] = [];
  private selectedBackground_?: NtpBackgroundData;
  indicatorModel?: StepIndicatorModel;

  private finalized_: boolean = false;
  private imageIsLoading_: boolean = false;

  private metricsManager_: ModuleMetricsManager;
  private ntpBackgroundProxy_: NtpBackgroundProxy;

  constructor() {
    super();

    this.subtitle = loadTimeData.getString('ntpBackgroundDescription');
    this.ntpBackgroundProxy_ = NtpBackgroundProxyImpl.getInstance();
    this.metricsManager_ =
        new ModuleMetricsManager(NtpBackgroundMetricsProxyImpl.getInstance());
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedBackground_')) {
      this.onSelectedBackgroundChange_();
    }
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
    if (this.backgrounds_.length === 0) {
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

  protected isSelectedBackground_(background: NtpBackgroundData) {
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

  protected onBackgroundPreviewTransitionEnd_() {
    // Whenever the #backgroundPreview transitions to a non-active, hidden
    // state, remove the background image. This way, when the element
    // transitions back to active, the previous background is not displayed.
    if (!this.$.backgroundPreview.classList.contains('active')) {
      this.$.backgroundPreview.style.backgroundImage = '';
    }
  }

  private announceA11y_(text: string) {
    getAnnouncerInstance().announce(text);
  }

  protected onBackgroundClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    this.selectedBackground_ = this.backgrounds_[index]!;
    this.metricsManager_.recordClickedOption();
    this.announceA11y_(this.i18n(
        'ntpBackgroundPreviewUpdated', this.selectedBackground_.title));
  }

  protected onBackgroundKeyUp_(e: KeyboardEvent) {
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

  protected onBackgroundPointerDown_(e: Event) {
    (e.currentTarget as HTMLElement).classList.remove(KEYBOARD_FOCUSED_CLASS);
  }

  protected onNextClicked_() {
    this.finalized_ = true;

    if (this.selectedBackground_ && this.selectedBackground_.id > -1) {
      this.ntpBackgroundProxy_.setBackground(this.selectedBackground_.id);
    } else {
      this.ntpBackgroundProxy_.clearBackground();
    }
    this.metricsManager_.recordGetStarted();
    navigateToNextStep();
  }

  protected onSkipClicked_() {
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
