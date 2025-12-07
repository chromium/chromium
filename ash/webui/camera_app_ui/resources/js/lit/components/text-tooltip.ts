// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  classMap,
  css,
  html,
  LitElement,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {checkInstanceof} from '../../assert.js';
import {DEFAULT_STYLE} from '../styles.js';

export class TextTooltip extends LitElement {
  static override styles = [
    DEFAULT_STYLE,
    css`
      #tooltip {
        background: var(--cros-sys-on_surface);
        border-radius: 2px;
        color: var(--cros-sys-inverse_on_surface);
        font: var(--cros-annotation-1-font);
        opacity: 0;
        padding: 2px 8px;
        pointer-events: none;
        position: absolute;
        white-space: nowrap;
        z-index: 100;
      }
      #tooltip.visible {
        opacity: 1;
        transition: opacity 350ms ease-out 1500ms;
      }
    `,
  ];

  static override properties: PropertyDeclarations = {
    target: {attribute: false},
    anchorTarget: {attribute: false},
  };

  /**
   * The tooltip content is the `target`'s aria-label.
   */
  target: Element|null = null;

  /**
   * The tooltip is anchored to `anchorTarget`.
   *
   * Typically the `anchorTarget` is the same as `target`.
   */
  anchorTarget: Element|null = null;

  private get tooltipElement() {
    return checkInstanceof(
        this.renderRoot.querySelector('#tooltip'), HTMLElement);
  }

  private readonly position = () => {
    if (this.anchorTarget === null || this.tooltipElement === null) {
      return;
    }

    const anchorRect = this.anchorTarget.getBoundingClientRect();
    const rect = this.tooltipElement.getBoundingClientRect();

    const [edgeMargin, elementMargin] = [5, 8];
    let top = anchorRect.top - rect.height - elementMargin;
    if (top < edgeMargin) {
      top = anchorRect.bottom + elementMargin;
    }
    this.tooltipElement.attributeStyleMap.set('top', CSS.px(top));

    // Center over the active element but avoid touching edges.
    const activeElementCenter = anchorRect.left + anchorRect.width / 2;
    const left = Math.min(
        Math.max(activeElementCenter - rect.width / 2, edgeMargin),
        document.body.offsetWidth - rect.width - edgeMargin);
    this.tooltipElement.attributeStyleMap.set('left', CSS.px(Math.round(left)));
  };

  private getContent() {
    return this.target?.getAttribute('aria-label') ?? null;
  }

  override connectedCallback(): void {
    super.connectedCallback();
    if (!this.hasAttribute('aria-hidden')) {
      this.setAttribute('aria-hidden', 'true');
    }
    window.addEventListener('resize', this.position);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    window.removeEventListener('resize', this.position);
  }

  protected override updated(): void {
    // Need to do positioning here since the position depends on the tooltip
    // size, which can only be measured after DOM has updated.
    this.position();
  }

  override render(): RenderResult {
    const content = this.getContent();
    const classes = {visible: this.anchorTarget !== null && content !== null};
    return html`
      <div id="tooltip" class=${classMap(classes)}>
        ${content}
      </div>
    `;
  }
}

window.customElements.define('text-tooltip', TextTooltip);

declare global {
  interface HTMLElementTagNameMap {
    'text-tooltip': TextTooltip;
  }
}
