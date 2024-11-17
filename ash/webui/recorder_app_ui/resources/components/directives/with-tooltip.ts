// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/tooltip/tooltip.js';

import {Tooltip} from 'chrome://resources/cros_components/tooltip/tooltip.js';
import {
  AsyncDirective,
  directive,
  DirectiveParameters,
  ElementPart,
  nothing,
  PartInfo,
  PartType,
} from 'chrome://resources/mwc/lit/index.js';

import {
  assert,
  assertExists,
  assertInstanceof,
} from '../../core/utils/assert.js';

class WithTooltip extends AsyncDirective {
  private tooltip: Tooltip|null = null;

  constructor(partInfo: PartInfo) {
    super(partInfo);
    assert(
      partInfo.type === PartType.ELEMENT,
      'The `withTooltip` directive must be used in element tag',
    );
  }

  private createTooltip(element: HTMLElement): Tooltip {
    const tooltip = new Tooltip();
    tooltip.ariaHidden = 'true';
    tooltip.anchorElement = element;
    return tooltip;
  }

  override update(part: ElementPart, [label]: DirectiveParameters<this>) {
    if (!this.isConnected) {
      return this.render(label);
    }

    const element = assertInstanceof(part.element, HTMLElement);
    if (this.tooltip === null) {
      this.tooltip = this.createTooltip(element);
      document.body.appendChild(this.tooltip);
    }

    if (label === undefined) {
      this.tooltip.label = assertExists(
        element.ariaLabel,
        '`aria-label` must be set when using `withTooltip()` without argument',
      );
    } else {
      this.tooltip.label = label;
    }

    return this.render(label);
  }

  protected override disconnected(): void {
    if (this.tooltip !== null) {
      document.body.removeChild(this.tooltip);
    }
  }

  protected override reconnected(): void {
    if (this.tooltip !== null) {
      document.body.appendChild(this.tooltip);
    }
  }

  override render(_ = '') {
    return nothing;
  }
}

/**
 * The directive sets up tooltip when the element is focused or hovered. If not
 * specified, the tooltip string is set to the aria-label of the element.
 */
export const withTooltip = directive(WithTooltip);
