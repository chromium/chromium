// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/icon_from_table.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {DragEventSource} from '//resources/mojo/ui/base/dragdrop/mojom/drag_drop_types.mojom-webui.js';
import {IconTable} from '/shared/icon_table.js';
import {LhsChipIdentifier, SecurityChipRole, SecurityLevel} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {SecurityChipState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './location_icon.css.js';
import {getHtml} from './location_icon.html.js';
import {PointerProxyImpl} from './pointer_proxy.js';
import {TimerHelper} from './timer_helper.js';

export interface LocationIconElement {
  $: {
    container: HTMLButtonElement,
  };
}

const LocationIconElementBase = HelpBubbleMixinLit(CrLitElement);

export class LocationIconElement extends LocationIconElementBase {
  protected getAccessibilityRole_(): string {
    return this.state.accessibilityState.role === SecurityChipRole.kImage ?
        'img' :
        'button';
  }
  static get is() {
    return 'location-icon';
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
      clickable: {
        type: Boolean,
        reflect: true,
      },
      isDangerous: {
        type: Boolean,
        reflect: true,
        attribute: 'is-dangerous',
      },
      hasText: {
        type: Boolean,
        reflect: true,
        attribute: 'has-text',
      },
      isTextDangerous: {
        type: Boolean,
        reflect: true,
        attribute: 'is-text-dangerous',
      },
      glowUpEnabled: {type: Boolean},
      glowUpActive: {
        type: Boolean,
        reflect: true,
        attribute: 'glow-up-active',
      },
    };
  }

  accessor state: SecurityChipState = {
    icon: {handleId: 0n},
    securityLevel: 0,
    text: '',
    tooltip: '',
    accessibilityState: {
      role: SecurityChipRole.kButton,
      label: '',
      description: '',
    },
    isClickable: false,
    isTextDangerous: false,
    isVisible: true,
    isContextMenuVisible: false,
  };

  accessor clickable: boolean = false;

  // True when the SecurityLevel is kDangerous (e.g. expired cert). The text
  // might still be "Not secure" in this state, which kWarning also has.
  accessor isDangerous: boolean = false;

  // True if the chip should display text alongside the icon. This drives CSS
  // rules that manage the expanded pill shape and padding.
  accessor hasText: boolean = false;

  // True specifically when the text is exactly "Dangerous" (e.g. Malware).
  // This is a higher alert state than just isDangerous.
  accessor isTextDangerous: boolean = false;

  accessor glowUpEnabled: boolean = loadTimeData.getBoolean('enableGlowUp');
  accessor glowUpActive: boolean = false;

  private dragStartX_: number = 0;
  private dragStartY_: number = 0;
  private isDragging_: boolean = false;
  private activePointerId_: number|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  private isAnimating_: boolean = false;
  private animationTimer_: TimerHelper = new TimerHelper();
  private isSecureIcon_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.registerHelpBubble('kLocationIconElementId', this.$.container, {
      onHighlightChanged: (highlighted: boolean) => {
        // Manually toggle the DOM attribute to bypass Lit's asynchronous
        // update batching, ensuring the style updates synchronously without
        // a 1-frame tear against the fast native IPCs.
        this.toggleAttribute('anchor-highlighted', highlighted);
      },
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.unregisterHelpBubble('kLocationIconElementId');
    this.eventTracker_.removeAll();
    this.animationTimer_.clearTimeout();
    this.isAnimating_ = false;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('state')) {
      this.clickable = this.state.isClickable;
      this.isDangerous = this.state.securityLevel === SecurityLevel.kDangerous;
      this.hasText = !!this.state.text;
      this.isTextDangerous = this.state.isTextDangerous;

      const iconInfo = IconTable.getInstance().getIconInfo(this.state.icon);
      this.isSecureIcon_ =
          iconInfo?.urlOrName === 'webui-toolbar:page_info_custom';
    }

    this.glowUpActive = this.computeGlowUpActive_(changedProperties);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      const oldShowing = oldState ? oldState.isContextMenuVisible : false;
      const newShowing = this.state ? this.state.isContextMenuVisible : false;
      if (oldShowing !== newShowing) {
        this.onContextMenuVisibleChanged_();
      }
    }
  }

  private computeGlowUpActive_(changedProperties: PropertyValues<this>):
      boolean {
    if (!this.glowUpEnabled || !this.isSecureIcon_ ||
        this.state.securityLevel !== SecurityLevel.kSecure) {
      return false;
    }

    const oldState = changedProperties.get('state');
    const wasContextMenuVisible =
        oldState ? oldState.isContextMenuVisible : false;
    const isContextMenuClosing =
        wasContextMenuVisible && !this.state.isContextMenuVisible;

    return this.state.isContextMenuVisible || this.isAnimating_ ||
        isContextMenuClosing;
  }

  private onContextMenuVisibleChanged_() {
    this.isAnimating_ = true;
    this.requestUpdate();

    const duration = 150;  // 150ms to match the SVG animation duration

    this.animationTimer_.setTimeout(() => {
      this.isAnimating_ = false;
      this.requestUpdate();
    }, duration);
  }

  protected onPointerdown_(e: PointerEvent) {
    if (this.activePointerId_ !== null) {
      return;
    }

    // Only handle primary (left), auxiliary (middle), and secondary (right)
    // clicks.
    if (!this.clickable || ![0, 1, 2].includes(e.button)) {
      return;
    }

    // e.button === 1 evaluates whether the primary trigger was the auxiliary
    // (middle) button. e.buttons === 4 ensures that ONLY the auxiliary button
    // is physically depressed to prevent false positives from chorded
    // multi-finger clicks.
    const isMiddleClick = e.button === 1 && e.buttons === 4;
    BrowserProxyImpl.getInstance().toolbarUIHandler.onLhsChipMousePressed(
        LhsChipIdentifier.kLocationIcon, isMiddleClick);

    if (e.button === 0) {
      this.dragStartX_ = e.clientX;
      this.dragStartY_ = e.clientY;
      this.isDragging_ = false;
      this.activePointerId_ = e.pointerId;

      PointerProxyImpl.getInstance().setPointerCapture(
          this.$.container, e.pointerId);

      this.eventTracker_.add(
          this.$.container, 'pointermove',
          (e: PointerEvent) => this.onContainerPointerMove_(e));
      this.eventTracker_.add(
          this.$.container, 'pointerup', () => this.onContainerPointerUp_());
      this.eventTracker_.add(
          this.$.container, 'pointercancel',
          () => this.onContainerPointerCancel_());
      this.eventTracker_.add(
          this.$.container, 'lostpointercapture',
          () => this.onContainerLostPointerCapture_());
    }
  }

  protected onContainerPointerMove_(e: PointerEvent) {
    if (this.activePointerId_ === null || this.isDragging_) {
      return;
    }

    const dx = e.clientX - this.dragStartX_;
    const dy = e.clientY - this.dragStartY_;
    const dragThresholdPx = 8;
    if (dx * dx + dy * dy >= dragThresholdPx * dragThresholdPx) {
      this.isDragging_ = true;
      const source = e.pointerType === 'touch' ? DragEventSource.kTouch :
                                                 DragEventSource.kMouse;
      BrowserProxyImpl.getInstance().toolbarUIHandler.onLhsChipDrag(
          LhsChipIdentifier.kLocationIcon, source);
    }
  }

  protected onContainerPointerUp_() {
    this.finishDrag_();
  }

  protected onContainerPointerCancel_() {
    this.finishDrag_();
  }

  protected onContainerLostPointerCapture_() {
    this.finishDrag_();
  }

  private finishDrag_() {
    if (this.activePointerId_ !== null) {
      PointerProxyImpl.getInstance().releasePointerCapture(
          this.$.container, this.activePointerId_);
      this.activePointerId_ = null;
    }
    this.eventTracker_.removeAll();
  }

  protected onClick_(e: PointerEvent) {
    if (this.clickable && !this.isDragging_) {
      // Note: Both 'click' and 'contextmenu' events are dispatched using
      // PointerEvents. Keyboard clicks (Enter/Space) also dispatch
      // PointerEvents, but they have an empty pointerType (""). We only want
      // to suppress true pointer interactions (mouse, touch, pen).
      BrowserProxyImpl.getInstance().toolbarUIHandler.onLhsChipClicked(
          LhsChipIdentifier.kLocationIcon, e.pointerType !== '');
    }
  }

  protected onContextmenu_(e: PointerEvent) {
    // In Native Views, LocationIconView::IsTriggerableEvent overrides the base
    // button behavior. It explicitly filters out middle-clicks (which trigger
    // "Paste-and-Go" on Linux instead of opening the bubble), but it falls
    // through to IconLabelBubbleView::IsTriggerableEvent, which intentionally
    // returns true for all other mouse events (including right-clicks).
    // Therefore, in Native Views, right-clicking the security chip legitimately
    // triggers the Page Info bubble. We explicitly forward the contextmenu
    // event to onClick_ here to maintain strict parity with that behavior.
    e.preventDefault();
    this.onClick_(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'location-icon': LocationIconElement;
  }
}

customElements.define(LocationIconElement.is, LocationIconElement);
