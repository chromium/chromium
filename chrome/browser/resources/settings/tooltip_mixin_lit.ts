// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides common shared tooltip behavior used in various
 * settings pages for Lit components.
 */

import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {dedupingMixin} from 'chrome://resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export interface TooltipMixinLitInterface {
  showTooltipAtTarget(tooltip: CrTooltipElement, target: Element): void;
}

export const TooltipMixinLit = dedupingMixin(
    <T extends Constructor<HTMLElement>>(superClass: T): T&
    Constructor<TooltipMixinLitInterface> => {
      class TooltipMixinLit extends superClass implements
          TooltipMixinLitInterface {
        showTooltipAtTarget(tooltip: CrTooltipElement, target: Element) {
          if (!tooltip.for) {
            // In the case that the tooltip and target are not associated with
            // the for property, manually set the target of the tooltip and
            // update its position.
            tooltip.target = target;
            tooltip.updatePosition();
          }
          const hide = () => {
            tooltip.hide();
            target.removeEventListener('mouseleave', hide);
            target.removeEventListener('blur', hide);
            target.removeEventListener('click', hide);
            tooltip.removeEventListener('mouseenter', hide);
          };
          target.addEventListener('mouseleave', hide);
          target.addEventListener('blur', hide);
          target.addEventListener('click', hide);
          tooltip.addEventListener('mouseenter', hide);
          tooltip.show();
        }
      }

      return TooltipMixinLit;
    });
