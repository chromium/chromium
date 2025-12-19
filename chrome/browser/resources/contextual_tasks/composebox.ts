// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';
import './onboarding_tooltip.js';

import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import type {ContextualTasksOnboardingTooltipElement} from './onboarding_tooltip.js';

export interface ContextualTasksComposeboxElement {
  $: {
    composebox: ComposeboxElement,
    onboardingTooltip: ContextualTasksOnboardingTooltipElement,
  };
}

export class ContextualTasksComposeboxElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-composebox';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      isZeroState: {
        type: Boolean,
        reflect: true,
      },
      isSidePanel: {
        type: Boolean,
        reflect: true,
      },
      composeboxHeight_: {type: Number},
      composeboxDropdownHeight_: {type: Number},
      isComposeboxFocused_: {
        type: Boolean,
        reflect: true,
      },
      showContextMenu_: {
        type: Boolean,
        value: loadTimeData.getBoolean('composeboxShowContextMenu'),
      },
      tabSuggestions_: {type: Array},
      showOnboardingTooltip_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showOnboardingTooltip'),
      },
    };
  }

  accessor isZeroState: boolean = false;
  accessor isSidePanel: boolean = false;
  protected accessor composeboxHeight_: number = 0;
  protected accessor composeboxDropdownHeight_: number = 0;
  protected accessor isComposeboxFocused_: boolean = false;
  protected accessor showContextMenu_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor showOnboardingTooltip_: boolean =
      loadTimeData.getBoolean('showOnboardingTooltip');
  private eventTracker_: EventTracker = new EventTracker();
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private searchboxListenerIds_: number[] = [];
  private onboardingTooltipIsVisible_: boolean = false;
  private numberOfTimesTooltipShown_: number = 0;
  private readonly maximumTimesTooltipShown_: number = loadTimeData.getInteger(
      'composeboxShowOnboardingTooltipSessionImpressionCap');
  private isOnboardingTooltipDismissCountBelowCap_: boolean =
      loadTimeData.getBoolean('isOnboardingTooltipDismissCountBelowCap');
  private userDismissedTooltip_: boolean = false;

  constructor() {
    super();
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.searchboxListenerIds_.push(
        this.searchboxCallbackRouter_.onTabStripChanged.addListener(
            this.refreshTabSuggestions_.bind(this)));

    this.refreshTabSuggestions_();

    const composebox = this.$.composebox;
    if (composebox) {
      this.eventTracker_.add(composebox, 'composebox-focus-in', () => {
        this.isComposeboxFocused_ = true;
      });
      this.eventTracker_.add(composebox, 'composebox-focus-out', () => {
        this.isComposeboxFocused_ = false;
        if (composebox.animationState === GlowAnimationState.SUBMITTING ||
            composebox.animationState === GlowAnimationState.LISTENING) {
          return;
        }
        composebox.animationState = GlowAnimationState.NONE;
      });
      this.eventTracker_.add(composebox, 'composebox-submit', () => {
        // Clear the composebox text after submitting.
        this.clearInputAndFocus();
      });
      this.eventTracker_.add(
          composebox, 'composebox-resize', (e: CustomEvent) => {
            if (e.detail.carouselHeight !== undefined) {
              composebox.style.setProperty(
                  '--carousel-height', `${e.detail.carouselHeight}px`);
            }
            if (e.detail.height !== undefined) {
              this.composeboxHeight_ = e.detail.height;
            }
          });

      this.eventTracker_.add(
          composebox.getDropTarget(), 'on-context-files-changed', () => {
            this.updateTooltipVisibility_();
          });

      // Initial check.
      this.updateTooltipVisibility_();
    }
  }

  private updateTooltipVisibility_() {
    if (!loadTimeData.getBoolean('showOnboardingTooltip')) {
      return;
    }

    const tooltip = this.$.onboardingTooltip;
    if (!tooltip) {
      return;
    }

    if (!tooltip.target) {
      tooltip.target = this.$.composebox;
    }

    if (this.onboardingTooltipIsVisible_ && !this.$.composebox.getHasAutomaticActiveTabChipToken()) {
        tooltip.hide();
        this.onboardingTooltipIsVisible_ = false;
    } else if(this.$.composebox.getHasAutomaticActiveTabChipToken()){
      const shouldShow = this.shouldShowOnboardingTooltip();
      if (shouldShow) {
        tooltip.show();
        this.numberOfTimesTooltipShown_++;
        this.onboardingTooltipIsVisible_ = true;
      }
    }
    tooltip.shouldShow = this.onboardingTooltipIsVisible_;
  }

  private shouldShowOnboardingTooltip(): boolean {
    return this.showOnboardingTooltip_ &&
        this.numberOfTimesTooltipShown_ < this.maximumTimesTooltipShown_
        && this.isOnboardingTooltipDismissCountBelowCap_ &&
        !this.userDismissedTooltip_;
  }

  protected onTooltipDismissed_() {
    this.userDismissedTooltip_ = true;
    this.onboardingTooltipIsVisible_ = false;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.searchboxListenerIds_.forEach(
        id => assert(this.searchboxCallbackRouter_.removeListener(id)));
    this.searchboxListenerIds_ = [];
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected async refreshTabSuggestions_() {
    const {tabs} = await this.searchboxHandler_.getRecentTabs();
    this.tabSuggestions_ = [...tabs];
    this.updateTooltipVisibility_();
  }

  clearInputAndFocus(): void {
    // Clear text from composebox and focus.
    this.$.composebox.clearInput();
    this.$.composebox.focusInput();
    this.$.composebox.clearAutocompleteMatches();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-composebox': ContextualTasksComposeboxElement;
  }
}

customElements.define(
    ContextualTasksComposeboxElement.is, ContextualTasksComposeboxElement);
