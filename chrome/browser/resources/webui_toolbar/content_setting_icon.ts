// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '/shared/icons.js';

import {assertNotReachedCase} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {ContentSettingImageState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {ContentSettingImageType} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './content_setting_icon.css.js';
import {getHtml} from './content_setting_icon.html.js';
import {HelpBubbleAnchorMixin, setHasHelpBubble} from './toolbar_button.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

// Duration (in ms) for the fade/slide animation of the chip label, matching
// kIconLabelFadeAnimationDurationMs in views::IconLabelBubbleView and
// --toolbar-chip-expand-duration in CSS.
const FADE_ANIMATION_DURATION_MS = 600;

// Duration (in ms) to hold the label visible before collapsing after a bubble
// closes or pointer interaction finishes, matching SetUpForInOutAnimation() in
// views::IconLabelBubbleView.
const COLLAPSE_HOLD_DURATION_MS = 1800;

// Delay (in ms) before the label automatically begins collapsing when running
// the in-out animation (appearance of label: 600ms + statically showing label:
// 1800ms).
const AUTO_COLLAPSE_DELAY_MS =
    FADE_ANIMATION_DURATION_MS + COLLAPSE_HOLD_DURATION_MS;

// Total duration (in ms) to statically display the label before auto-collapsing
// when prefers-reduced-motion is active. Matches the full slide animation
// duration in views::IconLabelBubbleView::SetUpForInOutAnimation().
const REDUCED_MOTION_AUTO_COLLAPSE_DELAY_MS =
    2 * FADE_ANIMATION_DURATION_MS + COLLAPSE_HOLD_DURATION_MS;

export interface ContentSettingIconElement {
  $: {
    chip: ToolbarChipButtonElement,
    label: HTMLElement,
  };
}

const ContentSettingIconElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class ContentSettingIconElement extends ContentSettingIconElementBase {
  static get is() {
    return 'content-setting-icon';
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
      trackedHighlighted: {type: Boolean},
      shouldShowLabel_: {
        type: Boolean,
        reflect: true,
        attribute: 'should-show-label',
      },
      suppressTransitions_: {
        type: Boolean,
      },
    };
  }

  accessor state: ContentSettingImageState = {
    type: ContentSettingImageType.kCookies,
    isBlocked: false,
    tooltip: '',
    accessibilityString: '',
    shouldRunAnimation: false,
    explanatoryString: '',
    identifier: {
      nativeIdentifier: '',
      secondaryIdentifier: '',
    },
  };

  accessor trackedHighlighted: boolean = false;

  protected accessor shouldShowLabel_: boolean = false;

  // Used to instantly neutralize CSS transitions when snapping the chip to its
  // fully expanded state when the bubble opens so the chip shows its full,
  // unclipped label rather than freezing partially expanded.
  protected accessor suppressTransitions_: boolean = false;

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private registerHelpBubbleController_: AbortController|null = null;
  private collapseTimerId_: number|null = null;

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.clearCollapseTimer_();
    this.shouldShowLabel_ = false;
    this.suppressTransitions_ = false;
    if (this.registerHelpBubbleController_) {
      this.registerHelpBubbleController_.abort();
      this.registerHelpBubbleController_ = null;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('state')) {
      this.handleAnimationTrigger_(changedProperties.get('state'));
    }

    if (changedProperties.has('trackedHighlighted')) {
      this.handleBubbleVisibilityChanged_(
          changedProperties.get('trackedHighlighted'));
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      const oldId = oldState?.identifier?.nativeIdentifier;
      const newId = this.state.identifier?.nativeIdentifier;

      // Only change registration when we see a new ID (like the initial state
      // message), not on ordinary state messages.
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

  /**
   * Processes new incoming state from C++ to determine if the CSS expansion
   * animation should be triggered.
   */
  private handleAnimationTrigger_(oldState?: ContentSettingImageState) {
    // Start the animation when C++ tells us to (and there is an explanatory
    // label string to show, matching Native Views `if (string_id)`), but only
    // if it's a new request (to ignore spurious identical backend state
    // updates).
    if (this.state.shouldRunAnimation && this.state.explanatoryString &&
        (!oldState || !oldState.shouldRunAnimation)) {
      // Enable CSS transitions so the expansion physically animates.
      this.suppressTransitions_ = false;
      this.clearCollapseTimer_();

      // Defer setting `shouldShowLabel_` until after the element has initially
      // rendered without it. This ensures the browser has a layout to
      // transition FROM, allowing CSS keyframes/transitions to fire.
      setTimeout(() => {
        if (!this.isConnected) {
          // Bail out if the element was detached from the DOM during the
          // async tick (e.g. tab closed or navigated away). It won't be
          // reattached, so we don't need to try and schedule timers for it.
          return;
        }
        // Force a synchronous style/layout calculation (reflow) so the browser
        // commits the initial collapsed (`max-width: 0`) style before
        // `shouldShowLabel_ = true` updates it (`setTimeout(0)` can fire before
        // the next animation frame's style pass).
        this.$.label.getBoundingClientRect();
        this.shouldShowLabel_ = true;
        this.startCollapseTimer_();
      }, 0);
    }
  }

  /**
   * Locks the expansion layout statically if the bubble is actively open, or
   * resumes the collapse sequence if the bubble is dismissed.
   */
  private handleBubbleVisibilityChanged_(oldTrackedHighlighted?: boolean) {
    // When the bubble is opened, pause collapse and snap the chip fully open so
    // it shows the complete, unclipped label while highlighted. When closed,
    // resume smooth transitions and collapse after a hold delay.
    if (this.trackedHighlighted) {
      this.clearCollapseTimer_();
      this.suppressTransitions_ = true;
      if (this.isLabelVisibleOrAnimating_()) {
        this.shouldShowLabel_ = true;
      }
    } else if (oldTrackedHighlighted) {
      this.suppressTransitions_ = false;
      if (this.shouldShowLabel_) {
        this.startCollapseTimer_(COLLAPSE_HOLD_DURATION_MS);
      }
    }
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
      this.registerHelpBubble(newId, this.$.chip, {
        secondaryId: this.state.identifier?.secondaryIdentifier || undefined,
        onHighlightChanged: (highlighted: boolean) => {
          this.trackedHighlighted = highlighted;
        },
        onHelpBubbleShown: () => setHasHelpBubble(this, true),
        onHelpBubbleHidden: () => setHasHelpBubble(this, false),
      });
      this.registerHelpBubbleController_ = null;
    }
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.state.tooltip);
  }

  override focus() {
    this.$.chip.focus();
  }

  protected getIconName_(): string {
    const iconType = this.state.type;
    const blocked = this.state.isBlocked;
    let iconName = '';

    switch (iconType) {
      case ContentSettingImageType.kCookies:
        iconName = blocked ? 'database_off' : 'database';
        break;
      case ContentSettingImageType.kImages:
        iconName = blocked ? 'hide_image' : 'photo';
        break;
      case ContentSettingImageType.kJavaScript:
        iconName = blocked ? 'code_off' : 'code';
        break;
      case ContentSettingImageType.kMixedScript:
        iconName = blocked ? 'warning_off' : 'warning';
        break;
      case ContentSettingImageType.kSound:
        iconName = blocked ? 'volume_off' : 'volume_up';
        break;
      case ContentSettingImageType.kAds:
        iconName = blocked ? 'ad_off' : 'ad';
        break;
      case ContentSettingImageType.kGeolocation:
        iconName = blocked ? 'location_off' : 'location_on';
        break;
      case ContentSettingImageType.kProtocolHandlers:
        iconName = blocked ? 'protocol_handler_off' : 'protocol_handler';
        break;
      case ContentSettingImageType.kMidiSysex:
        iconName = blocked ? 'piano_off' : 'piano';
        break;
      case ContentSettingImageType.kAutomaticDownloads:
        iconName = blocked ? 'file_download_off' : 'download';
        break;
      case ContentSettingImageType.kClipboardReadWrite:
        iconName = blocked ? 'content_paste_off' : 'content_paste';
        break;
      case ContentSettingImageType.kMediaStream:
        iconName = blocked ? 'videocam_off' : 'videocam';
        break;
      case ContentSettingImageType.kNotifications:
        iconName = blocked ? 'notifications_off' : 'notifications';
        break;
      case ContentSettingImageType.kSensors:
        iconName = blocked ? 'sensors_off' : 'sensors';
        break;
      case ContentSettingImageType.kStorageAccess:
        iconName = blocked ? 'vr180_create2d_off' : 'vr180_create2d';
        break;
      case ContentSettingImageType.kPopups:
        iconName = blocked ? 'iframe_off' : 'iframe';
        break;
      case ContentSettingImageType.kFramebust:
        iconName = blocked ? 'open_in_new_off' : 'open_in_new';
        break;
      // <if expr="is_chromeos">
      case ContentSettingImageType.kSmartCard:
        // Indicator shows only when at least one connection is active, hence no
        // need for the off icon.
        iconName = 'smart_card_reader';
        break;
      // </if>
      // <if expr="is_win">
      case ContentSettingImageType.kProtectedMediaIdentifier:
        iconName = blocked ? 'sync_saved_locally_off' : 'sync_saved_locally';
        break;
      // </if>
      default:
        assertNotReachedCase(iconType);
    }
    return iconName ? `webui-toolbar-shared:${iconName}` : '';
  }

  protected getAriaLabel_(): string {
    return this.state.accessibilityString || this.state.tooltip;
  }

  protected showContentSettingsBubble_(e: PointerEvent) {
    // Keyboard synthetic clicks generate PointerEvents with an empty
    // pointerType in WebUI, whereas natural pointer clicks have a valid
    // pointerType (e.g., 'mouse', 'touch', 'pen').
    const isPointerInteraction = !!e.pointerType;
    this.browserProxy_.toolbarUIHandler.showContentSettingsBubble(
        this.state.type, isPointerInteraction);
  }

  private clearCollapseTimer_() {
    if (this.collapseTimerId_ !== null) {
      clearTimeout(this.collapseTimerId_);
      this.collapseTimerId_ = null;
    }
  }

  private startCollapseTimer_(delayMs?: number) {
    this.clearCollapseTimer_();
    this.suppressTransitions_ = false;

    if (delayMs === undefined) {
      const isReducedMotion =
          window.matchMedia('(prefers-reduced-motion: reduce)').matches;
      delayMs = isReducedMotion ? REDUCED_MOTION_AUTO_COLLAPSE_DELAY_MS :
                                  AUTO_COLLAPSE_DELAY_MS;
    }

    this.collapseTimerId_ = setTimeout(() => {
      this.collapseTimerId_ = null;
      this.shouldShowLabel_ = false;
    }, delayMs);
  }

  private isLabelVisibleOrAnimating_(): boolean {
    return this.shouldShowLabel_ || this.collapseTimerId_ !== null ||
        this.$.label.getAnimations().length > 0;
  }

  protected onClick_(e: PointerEvent) {
    this.showContentSettingsBubble_(e);
  }

  protected onAuxclick_(e: PointerEvent) {
    // Handles both middle and right clicks.
    this.showContentSettingsBubble_(e);
  }

  protected onContextmenu_(e: PointerEvent) {
    // Prevent the default browser context menu on right click. The right click
    // action is natively handled as opening the bubble, which we process in
    // onAuxclick_ instead to avoid double-triggering.
    e.preventDefault();
  }

  protected onPointerdown_() {
    if (this.isLabelVisibleOrAnimating_()) {
      // If the user presses down on the chip while it is animating (either
      // expanding or collapsing), cancel the collapse timer and keep
      // `shouldShowLabel_ = true` so the chip expands to its full width and
      // does not collapse while the pointer is held down.
      // Note: If the user presses down on the chip, drags the pointer outside
      // the chip, and releases without opening the bubble, `onPointerup_` will
      // not fire on the chip and the label will remain expanded, matching
      // Native Views (`ContentSettingImageView::OnMousePressed`).
      this.clearCollapseTimer_();
      this.shouldShowLabel_ = true;
    }

    this.browserProxy_.toolbarUIHandler.onContentSettingImagePointerDown(
        this.state.type);
  }

  protected onPointerup_() {
    if (!this.trackedHighlighted && this.shouldShowLabel_) {
      this.startCollapseTimer_(COLLAPSE_HOLD_DURATION_MS);
    }
  }

  protected onPointerenter_() {
    this.fire('chip-pointerenter');
  }

  protected onPointerleave_() {
    this.fire('chip-pointerleave');
  }

  protected onPointercancel_() {
    if (!this.trackedHighlighted && this.shouldShowLabel_) {
      this.startCollapseTimer_(COLLAPSE_HOLD_DURATION_MS);
    }
    this.fire('chip-pointercancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-setting-icon': ContentSettingIconElement;
  }
}

customElements.define(ContentSettingIconElement.is, ContentSettingIconElement);
