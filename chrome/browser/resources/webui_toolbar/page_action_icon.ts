// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/icon_from_table.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import './toolbar_chip_button.js';

import {skColorToRgba} from '//resources/js/color_utils.js';
import {ensureTransitionEndEvent} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {AnimationTracker} from '/shared/animation_tracker.js';
import {IconTable} from '/shared/icon_table.js';
import type {PageActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {PageActionId, PageActionTrigger} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './page_action_icon.css.js';
import {getHtml} from './page_action_icon.html.js';
import {getClickSourceType, HelpBubbleAnchorMixin, playIconAnimation, setHasHelpBubble} from './toolbar_button.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export interface PageActionIconElement {
  $: {
    button: ToolbarChipButtonElement,
  };
}

const PageActionIconElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class PageActionIconElement extends PageActionIconElementBase {
  static get is() {
    return 'page-action-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},

      // Draw a focus ring as if focused (e.g., the AI mode chip when the user
      // tabs through the Omnibox suggestions, keeping real focus in the text
      // field).
      forceFocusRing: {type: Boolean},

      // Some additional style attributes for the chip element.
      chipStyleOverride_: {type: String, state: true},
      isHighlighted: {type: Boolean},

      glowUpEnabled: {type: Boolean},

      activeIconAnimation_: {type: String, state: true},
    };
  }

  accessor state: PageActionState = {
    pageActionId: PageActionId.kActionAiMode,
    accessibleName: '',
    tooltipText: '',
    icon: {handleId: 0n},
    text: '',
    shouldShowChip: false,
    shouldAnimateChipIn: false,
    shouldAnimateChipOut: false,
    backgroundColorOverride: null,
    identifier: {
      nativeIdentifier: '',
      secondaryIdentifier: '',
    },
    isActive: false,
    iconAnimationToken: 0,
  };

  accessor forceFocusRing: boolean = false;
  protected accessor isHighlighted: boolean = false;

  protected accessor chipStyleOverride_: string|null = null;

  accessor glowUpEnabled: boolean =
      loadTimeData.getBoolean('enableBookmarkGlowUp');

  private accessor activeIconAnimation_: 'start'|'end'|'none' = 'none';
  private currentIconName_: string = '';
  private animatedIconColor_: string = '';
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private wasShowingChip_: boolean = false;
  private registerHelpBubbleController_: AbortController|null = null;
  // Tracks the active 'endEvent' handler for the SVG animation to allow
  // cleanup.
  private animationEndHandler_: (() => void)|null = null;
  // Reference to the SVG animation element that currently has a listener
  // attached.
  private animationElement_: Element|null = null;

  override focus() {
    this.$.button.focus();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.registerHelpBubbleController_) {
      this.registerHelpBubbleController_.abort();
      this.registerHelpBubbleController_ = null;
    }
    // Clean up animation event listeners on disconnect to prevent memory leaks
    // when dynamic page action icons are detached from the DOM.
    this.cleanupAnimationListener_();
    this.activeIconAnimation_ = 'none';
  }
  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      this.wasShowingChip_ =
          oldState?.pageActionId === this.state.pageActionId &&
          (oldState?.shouldShowChip ?? false);
      // If the icon animation token changed, it indicates a tab switch or
      // navigation. In this case, we suppress any transition icon animations
      // and reset the animation state to avoid carrying over animations.
      const isTabSwitch = !oldState ||
          this.state.iconAnimationToken !== oldState.iconAnimationToken;

      if (isTabSwitch) {
        this.cleanupAnimationListener_();
        this.activeIconAnimation_ = 'none';
      }
      if (this.state.backgroundColorOverride) {
        this.chipStyleOverride_ = `--toolbar-chip-bg-color: ${
            skColorToRgba(this.state.backgroundColorOverride)};`;
      } else {
        this.chipStyleOverride_ = null;
      }

      this.updateIconAnimationState_(isTabSwitch);
    }
  }

  private updateIconAnimationState_(isTabSwitch: boolean) {
    const newIconName =
        IconTable.getInstance().getIconName(this.state.icon) || '';
    const newIconColor =
        IconTable.getInstance().getIconColor(this.state.icon) || '';
    const oldIconName = this.currentIconName_;

    let playAnimation = false;
    // Only play the bookmark transition icon animation if the feature is
    // enabled, we have a previous icon state to compare against, and this
    // update is not due to a tab switch.
    if (this.state.pageActionId === PageActionId.kActionBookmarkThisTab &&
        this.glowUpEnabled && oldIconName && !isTabSwitch) {
      if (oldIconName !== newIconName) {
        if (oldIconName === 'webui-toolbar:star' &&
            newIconName === 'webui-toolbar:star_filled') {
          this.activeIconAnimation_ = 'start';
          playAnimation = true;
        } else if (
            oldIconName === 'webui-toolbar:star_filled' &&
            newIconName === 'webui-toolbar:star') {
          this.activeIconAnimation_ = 'end';
          playAnimation = true;
        }
      }
    }

    this.animatedIconColor_ = newIconColor;

    if (newIconName) {
      this.currentIconName_ = newIconName;
    }

    if (playAnimation) {
      this.playIconAnimation_();
    }
  }
  protected shouldShowLabel_(): boolean {
    return this.state.shouldShowChip && !!this.state.text;
  }

  protected shouldAnimate_(): boolean {
    if (!this.state.text) {
      return false;
    }
    return this.state.shouldShowChip ?
        this.state.shouldAnimateChipIn :
        (this.wasShowingChip_ && this.state.shouldAnimateChipOut);
  }
  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('forceFocusRing')) {
      this.$.button.toggleAttribute('force-focus-ring', this.forceFocusRing);
    }
    if (changedProperties.has('state')) {
      this.toggleAttribute(
          'is-aim', this.state.pageActionId === PageActionId.kActionAiMode);
      const oldState = changedProperties.get('state');
      if (!oldState || oldState.shouldShowChip !== this.state.shouldShowChip) {
        const fireIpc = () => {
          this.browserProxy_.toolbarUIHandler.onPageActionChipShowingChanged(
              this.state.pageActionId);
        };

        if (!AnimationTracker.showAnimations) {
          fireIpc();
        } else {
          const button = this.$.button;
          button.addEventListener('transitionend', fireIpc, {once: true});
          ensureTransitionEndEvent(button);
        }
      }

      const oldId = oldState?.identifier?.nativeIdentifier;
      const newId = this.state.identifier?.nativeIdentifier;
      if (oldId !== newId) {
        if (this.registerHelpBubbleController_) {
          this.registerHelpBubbleController_.abort();
          this.registerHelpBubbleController_ = null;
        }
        if (oldId) {
          this.unregisterHelpBubble(oldId);
        }
        if (newId) {
          this.registerHelpBubble_(newId);
        }
      }
    }
  }

  protected isIconAnimating_(): boolean {
    return this.activeIconAnimation_ !== 'none';
  }

  protected getAnimatedIcon_(): string {
    return this.activeIconAnimation_ === 'start' ?
        'webui-toolbar:star_glow_up' :
        'webui-toolbar:star_filled_glow_up';
  }

  protected getAnimatedIconStyle_(): string|undefined {
    const color = this.animatedIconColor_;
    return color ? `color: ${color};` : undefined;
  }

  private async playIconAnimation_() {
    // Wait for Lit update to complete so the DOM has rendered the new
    // <cr-icon> containing the animated SVG, then wait for the cr-icon's
    // own update if necessary, and finally trigger the SVG animation.
    await this.updateComplete;
    const crIcon = this.shadowRoot.querySelector<CrLitElement>('#animatedIcon');
    if (crIcon) {
      await crIcon.updateComplete;
    }
    requestAnimationFrame(() => {
      playIconAnimation(this);
      this.listenToAnimationEnd_();
    });
  }

  /**
   * Listens to the SMIL animation end event on the SVG element and resets
   * the animation state to 'none' once finished, switching back to the static
   * icon.
   */
  private listenToAnimationEnd_() {
    this.cleanupAnimationListener_();

    const animationElement =
        this.shadowRoot.querySelector('#animatedIcon')
            ?.shadowRoot?.querySelector('animate, animateTransform');
    if (!animationElement) {
      this.activeIconAnimation_ = 'none';
      return;
    }
    this.animationElement_ = animationElement;
    this.animationEndHandler_ = () => {
      this.cleanupAnimationListener_();
      this.activeIconAnimation_ = 'none';
    };
    animationElement.addEventListener('endEvent', this.animationEndHandler_);
  }

  /**
   * Safely removes the animation 'endEvent' listener and clears references.
   */
  private cleanupAnimationListener_() {
    if (this.animationElement_ && this.animationEndHandler_) {
      this.animationElement_.removeEventListener(
          'endEvent', this.animationEndHandler_);
    }
    this.animationElement_ = null;
    this.animationEndHandler_ = null;
  }

  protected getAriaLabel_(): string {
    return this.state.accessibleName;
  }

  protected onClick_(e: Event) {
    this.browserProxy_.toolbarUIHandler.onPageActionClick(
        this.state.pageActionId, this.getPageActionTrigger_(e));
  }

  private getPageActionTrigger_(e: Event): PageActionTrigger {
    const sourceType = getClickSourceType(e);
    switch (sourceType) {
      case MenuSourceType.kTouch:
        return PageActionTrigger.kGesture;
      case MenuSourceType.kKeyboard:
        return PageActionTrigger.kKeyboard;
      case MenuSourceType.kMouse:
        return PageActionTrigger.kMouse;
      default:
        throw new Error('Unknown sourceType ' + sourceType);
    }
  }

  protected onPointerdown_() {
    this.browserProxy_.toolbarUIHandler.onPageActionPointerDown(
        this.state.pageActionId);
  }

  // TODO(crbug.com/489109708): Deduplicate help bubble tracking logic across
  // toolbar elements.
  private async registerHelpBubble_(newId: string) {
    this.registerHelpBubbleController_ = new AbortController();
    const signal = this.registerHelpBubbleController_.signal;

    const animations = this.getAnimations().filter(anim => {
      const timing = anim.effect?.getTiming();
      // Ignore infinite animations (e.g. pulsing for IPH).
      return timing?.iterations !== Infinity && timing?.duration !== Infinity;
    });

    // Wait for any animations to complete, so button is in final location.
    if (animations.length > 0) {
      try {
        await Promise.all(animations.map(a => a.finished));
      } catch (e) {
        // Ignore animation cancellation.
      }
    }

    if (!signal.aborted) {
      this.registerHelpBubble(newId, this.$.button, {
        secondaryId: this.state.identifier?.secondaryIdentifier || undefined,
        onHighlightChanged: (highlighted: boolean) => {
          this.isHighlighted = highlighted;
        },
        onHelpBubbleShown: () => setHasHelpBubble(this, true),
        onHelpBubbleHidden: () => setHasHelpBubble(this, false),
      });
      this.registerHelpBubbleController_ = null;
    }
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.state.tooltipText);
  }

  protected onPointerenter_() {
    this.fire('chip-pointerenter');
  }

  protected onPointerleave_() {
    this.fire('chip-pointerleave');
  }

  protected onPointercancel_() {
    this.fire('chip-pointercancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'page-action-icon': PageActionIconElement;
  }
}

customElements.define(PageActionIconElement.is, PageActionIconElement);
