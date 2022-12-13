// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides common shared tooltip behavior used in various
 * settings pages.
 */

import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export interface TooltipMixinInterface {
  showTooltipAtTarget(tooltip: PaperTooltipElement, target: EventTarget): void;
}

export const TooltipMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<TooltipMixinInterface> => {
      class TooltipMixin extends superClass {
        showTooltipAtTarget(tooltip: PaperTooltipElement, target: EventTarget) {
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

      return TooltipMixin;
    });
