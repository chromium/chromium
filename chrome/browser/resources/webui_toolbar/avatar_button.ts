// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '/shared/icon_from_table.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {HelpBubbleAnchorMixin, HighlightTracker} from '/shared/toolbar_button.js';
import type {AvatarControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {AvatarToolbarButtonState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import type {ToolbarAppElement} from './app.js';
import {getCss} from './avatar_button.css.js';
import {getHtml} from './avatar_button.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {OverflowableButtonState} from './overflowable_button.js';
import {OverflowableButtonMixin} from './overflowable_button.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export interface AvatarButtonElement {
  $: {
    button: ToolbarChipButtonElement,
    ellipsis: HTMLElement,
    text: HTMLElement,
  };
}

/**
 * How the label is currently displayed, reflected in the always-set
 * `label-mode` attribute. When the full label doesn't fit, the button's width
 * is set with an inline style, and its contents shrink or grow to fill it.
 */
type LabelMode =
    // The label text is displayed. If there isn't enough space for all of it,
    // CSS elides it, displaying as many characters as fit, followed by an
    // ellipsis.
    'text'|
    // There is label text, but only an ellipsis is displayed in its place.
    // Used when there's enough space for an ellipsis, but not for the first
    // character of the label and an ellipsis, since CSS would still display
    // part of the first character in that case, unlike Views.
    'ellipsis'|
    // There is label text, and space is reserved for it, but nothing is
    // displayed in that space, since there's not enough room for even an
    // ellipsis. The button looks the same as in the other modes with label
    // text, other than the label area being blank.
    'space-only'|
    // There is label text, but there's not enough space for any label area at
    // all, so it's hidden entirely. The button keeps the highlight color,
    // border, and outer margins it has when there's room for label text, but
    // its padding is removed, and its icon is centered in a circular highlight,
    // as space allows. See the `.icon-spacer` CSS rules.
    'collapsed'|
    // There's no label text (`state.text` is empty). The button is displayed
    // as a regular icon button.
    'no-text';

const LABEL_MODE_ATTRIBUTE = 'label-mode';

// Used to extract first grapheme of attached text, to determine if that,
// followed by an ellipsis, fits in available space.
const graphemeSegmenter =
    new Intl.Segmenter(undefined, {granularity: 'grapheme'});

const AvatarButtonElementBase =
    HelpBubbleAnchorMixin(OverflowableButtonMixin(CrLitElement));

export class AvatarButtonElement extends AvatarButtonElementBase {
  static get is() {
    return 'avatar-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...super.properties,
      state: {type: Object},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.classList.add('initial-load');
    // Set an initial mode before the first render, so the attribute is always
    // set. updated() sets the correct mode and updates the inner button.
    if (!this.hasAttribute(LABEL_MODE_ATTRIBUTE)) {
      this.setAttribute(LABEL_MODE_ATTRIBUTE, 'no-text');
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('hasHelpBubble')) {
      BrowserProxyImpl.getInstance()
          .toolbarUIHandler.setAvatarButtonIphPromoShowing(this.hasHelpBubble);
    }
    if (changedProperties.has('state')) {
      // When the WebUI toolbar handles layout, the label mode is set during
      // layout, which is requested below (and by the mixin on the first state
      // update). This is only needed when that doesn't happen: in tests that
      // don't run layouts, and when `webUIToolbarFullyEnabled_` is false in
      // ToolbarAppElement, in which case layout is handled by Views.
      // This can be removed once `webUIToolbarFullyEnabled_` is always true,
      // updating tests as needed.
      if (!this.state.text) {
        this.setLabelMode_('no-text');
      } else if (this.getLabelMode_() === 'no-text') {
        this.setLabelMode_('text');
      }

      // These fields affect the preferred width, and the toolbar won't resize
      // on its own when the button is hidden or its width is constrained, so
      // request a layout.
      const oldState = changedProperties.get('state');
      if (oldState &&
          (oldState.text !== this.state.text ||
           oldState.state !== this.state.state ||
           oldState.hasLinearGradientRing !==
               this.state.hasLinearGradientRing)) {
        this.fire('request-layout');
      }
    }
  }

  override accessor state: AvatarControlState&OverflowableButtonState = {
    state: AvatarToolbarButtonState.kNormal,
    icon: {handleId: 0n},
    text: '',
    tooltip: '',
    accessibilityName: '',
    accessibilityDescription: '',
    enabled: true,
    hasLinearGradientRing: false,

    // This field is from OverflowableButtonState (in addition to
    // AvatarControlState), and is used by OverflowableButtonMixin.
    preventOverflow: false,
  };

  protected highlightTracker: HighlightTracker = new HighlightTracker();

  /**
   * Collapses the label, if there is one, and hides the button unless overflow
   * is prevented.
   */
  override setToMinWidth() {
    // Reset button to base state, clearing the width CSS property.
    this.setToPreferredWidth();

    if (this.state.text) {
      this.setLabelMode_('collapsed');
    }
    super.setToMinWidth();
  }

  /**
   * Roughly mimics the Views avatar button. Tries to show the button with its
   * full label. If that doesn't fit, shrinks the button to fit, and picks a
   * label mode depending on how much space is left for the label: 'text' (with
   * CSS eliding the label), 'ellipsis', or 'space-only'. If there's not enough
   * room for even an empty label area, switches to 'collapsed', and if that
   * doesn't fit either, hides the button entirely, unless overflow is
   * prevented. See `LabelMode` for details.
   */
  override expandUpToPreferredWidth() {
    const toolbarApp = this.getToolbarApp_();
    this.setToPreferredWidth();
    const availableWidth = toolbarApp.getAvailableWidth();
    if (availableWidth >= 0) {
      return;
    }

    if (this.state.text) {
      // Everything is measured before making any changes, so these all use
      // the layout from the getAvailableWidth() call above.
      const buttonWidth = this.$.button.getBoundingClientRect().width;
      const textWidth = this.$.text.getBoundingClientRect().width;
      const ellipsisWidth = this.$.ellipsis.getBoundingClientRect().width;
      const firstGraphemeWidth = this.getFirstGraphemeWidth_();

      // Round down so the button fits. Uses the exact available width, since
      // the rounded one can be off by up to half a pixel in either direction.
      const newButtonWidth =
          Math.floor(buttonWidth + toolbarApp.getExactAvailableWidth());
      const availableLabelWidth = textWidth - (buttonWidth - newButtonWidth);
      if (availableLabelWidth >= 0) {
        if (availableLabelWidth < ellipsisWidth) {
          this.setLabelMode_('space-only');
        } else if (availableLabelWidth < ellipsisWidth + firstGraphemeWidth) {
          this.setLabelMode_('ellipsis');
        }
        this.setButtonWidth_(newButtonWidth);
        return;
      }

      this.setLabelMode_('collapsed');
      if (toolbarApp.getAvailableWidth() >= 0) {
        // Grow to fill any extra space. The exact available width may be
        // slightly negative even when the rounded one isn't, in which case
        // leave the button at its natural width.
        const extraWidth = toolbarApp.getExactAvailableWidth();
        if (extraWidth > 0) {
          this.setButtonWidth_(Math.floor(
              this.$.button.getBoundingClientRect().width + extraWidth));
        }
        return;
      }
    }

    if (!this.shouldPreventOverflow()) {
      super.setToMinWidth();
    }
  }

  /**
   * Unconditionally shows the button at its natural width, with its full label
   * (if it has one).
   */
  override setToPreferredWidth() {
    this.$.button.style.width = '';
    this.setLabelMode_(this.state.text ? 'text' : 'no-text');
    super.setToPreferredWidth();
  }

  private getToolbarApp_(): ToolbarAppElement {
    const shadowRoot = this.getRootNode() as ShadowRoot;
    return shadowRoot.host as ToolbarAppElement;
  }

  /**
   * Returns the width of the label's first grapheme, as currently displayed.
   */
  private getFirstGraphemeWidth_(): number {
    // The label's only child is a text node, from the `.textContent` binding.
    const textNode = this.$.text.firstChild as Text | null;
    if (!textNode?.data) {
      return 0;
    }
    const firstGrapheme =
        graphemeSegmenter.segment(textNode.data)[Symbol.iterator]().next();
    if (!firstGrapheme.value) {
      return 0;
    }
    const range = document.createRange();
    range.setStart(textNode, 0);
    range.setEnd(textNode, firstGrapheme.value.segment.length);
    return range.getBoundingClientRect().width;
  }

  /**
   * Sets the width of the button, including its highlight. `width` should be
   * rounded down, so the button fits. Its contents shrink or grow to fit (see
   * the `#text`, `#ellipsis`, and `.icon-spacer` CSS rules).
   */
  private setButtonWidth_(width: number) {
    this.$.button.style.width = `${width}px`;
  }

  private getLabelMode_(): LabelMode {
    return this.getAttribute(LABEL_MODE_ATTRIBUTE) as LabelMode;
  }

  /**
   * Sets the label mode, and updates the inner button's `has-label` attribute,
   * which depends on whether there's a label area. These, and all other sizing
   * changes, are done synchronously rather than through Lit, since layout
   * measures widths immediately after each sizing call.
   */
  private setLabelMode_(mode: LabelMode) {
    this.setAttribute(LABEL_MODE_ATTRIBUTE, mode);
    // Without label text, the button always has its natural width.
    if (mode === 'no-text') {
      this.$.button.style.width = '';
    }
    this.$.button.toggleAttribute(
        'has-label', mode !== 'collapsed' && mode !== 'no-text');
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.state?.tooltip || '');
  }

  protected shouldPaintBorder(): boolean {
    return !!this.state.text &&
        this.state.state === AvatarToolbarButtonState.kGuestSession;
  }

  protected getButtonClass_(): string {
    const classes = [];
    if (this.hasHelpBubble) {
      classes.push('help-anchor-highlight');
    }
    if (this.state.text) {
      switch (this.state.state) {
        case AvatarToolbarButtonState.kSyncError:
          classes.push('highlight-sync-error');
          break;
        case AvatarToolbarButtonState.kGuestSession:
          classes.push('highlight-guest');
          break;
        case AvatarToolbarButtonState.kIncognitoProfile:
          classes.push('highlight-incognito');
          break;
        default:
          classes.push('highlight-default');
          break;
      }
    }
    return classes.join(' ');
  }

  protected onClick_(e: PointerEvent) {
    if (!this.highlightTracker.shouldSkipClick(e)) {
      TrackedElementManager.getInstance().notifyElementActivated(this);
      // TODO(behamilton): Log an error if this fails.
      BrowserProxyImpl.getInstance().toolbarUIHandler.showAvatarMenu();
    }
  }

  protected onMouseenter_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonHovered(
        true);
  }

  protected onMouseleave_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonHovered(
        false);
  }

  protected onFocusin_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonFocused(
        true);
  }

  protected onFocusout_() {
    BrowserProxyImpl.getInstance().toolbarUIHandler.setAvatarButtonFocused(
        false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'avatar-button': AvatarButtonElement;
  }
}

customElements.define(AvatarButtonElement.is, AvatarButtonElement);
