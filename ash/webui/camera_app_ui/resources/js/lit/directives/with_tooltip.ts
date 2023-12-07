// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  directive,
  Directive,
  ElementPart,
  nothing,
  PartInfo,
  PartType,
} from 'chrome://resources/mwc/lit/index.js';

import {assert, assertInstanceof} from '../../assert.js';
import * as tooltip from '../../tooltip.js';


class WithTooltip extends Directive {
  private firstUpdate = true;

  constructor(partInfo: PartInfo) {
    super(partInfo);
    assert(
        partInfo.type === PartType.ELEMENT,
        'The `withTooltip` directive must be used in element tag');
  }

  override update(part: ElementPart) {
    if (this.firstUpdate) {
      tooltip.setupElements([assertInstanceof(part.element, HTMLElement)]);
      this.firstUpdate = false;
    }
    return this.render();
  }

  override render() {
    return nothing;
  }
}

/**
 * The directive set up tooltip when the element is focused or hovered.
 */
export const withTooltip = directive(WithTooltip);
