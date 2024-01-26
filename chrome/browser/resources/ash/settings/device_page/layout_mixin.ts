// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for handling display layout, specifically
 *     edge snapping and collisions.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Constructor} from '../common/types.js';

import {getDisplayApi} from './device_page_browser_proxy.js';
import {DragMixin, DragMixinInterface, Position} from './drag_mixin.js';

import Bounds = chrome.system.display.Bounds;
import DisplayLayout = chrome.system.display.DisplayLayout;
import DisplayUnitInfo = chrome.system.display.DisplayUnitInfo;
import LayoutPosition = chrome.system.display.LayoutPosition;

export {Position};

export interface LayoutMixinInterface extends DragMixinInterface {
  /**
   * Array of display layouts.
   */
  layouts: DisplayLayout[];

  /**
   * Whether or not mirroring is enabled.
   */
  mirroring: boolean;

  initializeDisplayLayout(
      displays: DisplayUnitInfo[], layouts: DisplayLayout[]): void;


  /**
   * Called when a drag event occurs. Checks collisions and updates the layout.
   */
  updateDisplayBounds(id: string, newBounds: Bounds): Bounds;

  /**
   * Called when dragging ends. Sends the updated layout to chrome.
   */
  finishUpdateDisplayBounds(id: string): void;

  /**
   * Overloaded method for better typechecking depending on existence and
   * value of |notest| argument
   * @param notest Set to true if bounds may not be set.
   */
  getCalculatedDisplayBounds<T extends boolean>(displayId: string, notest: T):
      T extends false? Bounds: (Bounds|undefined);
  getCalculatedDisplayBounds(displayId: string): Bounds;

  getDisplayLayoutMapForTesting(): Map<string, DisplayLayout>;
}

export const LayoutMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<LayoutMixinInterface> => {
      const superClassBase = DragMixin(superClass);
      class LayoutMixinInternal extends superClassBase implements
          LayoutMixinInterface {
        static get properties() {
          return {
            layouts: Array,

            mirroring: {
              type: Boolean,
              value: false,
            },
          };
        }

        layouts: DisplayLayout[];
        mirroring: boolean;

        /**
         * The calculated bounds used for generating the div bounds.
         */
        private calculatedBoundsMap_: Map<string, Bounds> = new Map();
        private displayBoundsMap_: Map<string, Bounds> = new Map();
        private displayLayoutMap_: Map<string, DisplayLayout> = new Map();
        private dragBounds_: Bounds|undefined = undefined;
        private dragLayoutId_: string = '';
        private dragLayoutPosition_: LayoutPosition|undefined = undefined;
        private dragParentId_: string = '';

        getDisplayLayoutMapForTesting(): Map<string, DisplayLayout> {
          return this.displayLayoutMap_;
        }

        initializeDisplayLayout(
            displays: DisplayUnitInfo[], layouts: DisplayLayout[]): void {
          this.dragLayoutId_ = '';
          this.dragParentId_ = '';

          this.mirroring =
              displays.length > 0 && !!displays[0].mirroringSourceId;

          this.displayBoundsMap_.clear();
          for (const display of displays) {
            this.displayBoundsMap_.set(display.id, display.bounds);
          }
          this.displayLayoutMap_.clear();
          for (const layout of layouts) {
            this.displayLayoutMap_.set(layout.id, layout);
          }
          this.calculatedBoundsMap_.clear();
          for (const display of displays) {
            if (!this.calculatedBoundsMap_.has(display.id)) {
              const bounds = display.bounds;
              this.calculateBounds_(display.id, bounds.width, bounds.height);
            }
          }
        }

        updateDisplayBounds(id: string, newBounds: Bounds): Bounds {
          this.dragLayoutId_ = id;

          // Find the closest parent.
          const closestId = this.findClosest_(id, newBounds);
          assert(closestId);

          // Find the closest edge.
          const closestBounds = this.getCalculatedDisplayBounds(closestId);
          const layoutPosition =
              this.getLayoutPositionForBounds_(newBounds, closestBounds);

          // Snap to the closest edge.
          const snapPos =
              this.snapBounds_(newBounds, closestId, layoutPosition);
          newBounds.left = snapPos.x;
          newBounds.top = snapPos.y;

          // Calculate the new bounds and delta.
          const oldBounds =
              this.dragBounds_ || this.getCalculatedDisplayBounds(id);
          const deltaPos = {
            x: newBounds.left - oldBounds.left,
            y: newBounds.top - oldBounds.top,
          };

          // Check for collisions after snapping. This should not collide with
          // the closest parent.
          this.collideAndModifyDelta_(id, oldBounds, deltaPos);

          // If the edge changed, update and highlight it.
          if (layoutPosition !== this.dragLayoutPosition_ ||
              closestId !== this.dragParentId_) {
            this.dragLayoutPosition_ = layoutPosition;
            this.dragParentId_ = closestId;
            this.highlightEdge_(closestId, layoutPosition);
          }

          newBounds.left = oldBounds.left + deltaPos.x;
          newBounds.top = oldBounds.top + deltaPos.y;

          this.dragBounds_ = newBounds;

          return newBounds;
        }

        finishUpdateDisplayBounds(id: string): void {
          this.highlightEdge_('', undefined);  // Remove any highlights.
          if (id !== this.dragLayoutId_ || !this.dragBounds_ ||
              !this.dragLayoutPosition_) {
            return;
          }

          const layout = this.displayLayoutMap_.get(id);

          let orphanIds: string[];
          if (!layout || layout.parentId === '') {
            // Primary display. Set the calculated position to |dragBounds_|.
            this.setCalculatedDisplayBounds_(id, this.dragBounds_);

            // We cannot re-parent the primary display, so instead make all
            // other displays orphans and clear their calculated bounds.
            orphanIds = this.findChildren_(id, /* recurse= */ true);

            // Re-parent |dragParentId_|. It will be forced to parent to the
            // dragged display since it is the only non-orphan.
            this.reparentOrphan_(this.dragParentId_, orphanIds);
            orphanIds.splice(orphanIds.indexOf(this.dragParentId_), 1);
          } else {
            // All immediate children of |layout| will need to be re-parented.
            orphanIds = this.findChildren_(id, false /* do not recurse */);

            // When re-parenting to a descendant, also parent any immediate
            // child to drag display's current parent.
            let topLayout = this.displayLayoutMap_.get(this.dragParentId_);
            while (topLayout && topLayout.parentId !== '') {
              if (topLayout.parentId === id) {
                topLayout.parentId = layout.parentId;
                break;
              }
              topLayout = this.displayLayoutMap_.get(topLayout.parentId);
            }

            // Re-parent the dragged display.
            layout.parentId = this.dragParentId_;
            this.updateOffsetAndPosition_(
                this.dragBounds_, this.dragLayoutPosition_, layout);
          }

          // Update any orphaned children. This may cause the dragged display to
          // be re-attached if it was attached to a child.
          this.updateOrphans_(orphanIds);

          // Send the updated layouts.
          getDisplayApi().setDisplayLayout(this.layouts).then(() => {
            if (chrome.runtime.lastError) {
              console.error(
                  'setDisplayLayout Error: ' +
                  chrome.runtime.lastError.message);
            }
          });
        }

        /**
         * Overloaded method for better typechecking depending on existence and
         * value of |notest| argument
         * @param notest Set to true if bounds may not be set.
         */
        getCalculatedDisplayBounds<T extends boolean>(
            displayId: string, notest: T): T extends true?
            (Bounds|undefined): Bounds;
        getCalculatedDisplayBounds(displayId: string): Bounds;
        getCalculatedDisplayBounds(displayId: string, notest?: boolean): Bounds
            |undefined {
          const bounds = this.calculatedBoundsMap_.get(displayId);
          assert(notest || bounds);
          return bounds;
        }

        private setCalculatedDisplayBounds_(
            displayId: string, bounds: Bounds|undefined): void {
          assert(bounds);
          this.calculatedBoundsMap_.set(displayId, {...bounds});
        }

        /**
         * Re-parents all entries in |orphanIds| and any children.
         * @param orphanIds The list of ids affected by the move.
         */
        private updateOrphans_(orphanIds: string[]): void {
          const orphans = orphanIds.slice();
          for (let i = 0; i < orphanIds.length; ++i) {
            const orphan = orphanIds[i];
            const newOrphans = this.findChildren_(orphan, true /* recurse */);
            // If the dragged display was re-parented to one of its children,
            // there may be duplicates so merge the lists.
            for (let j = 0; j < newOrphans.length; ++j) {
              const o = newOrphans[j];
              if (!orphans.includes(o)) {
                orphans.push(o);
              }
            }
          }

          // Remove each orphan from the list as it is re-parented so that
          // subsequent orphans can be parented to it.
          while (orphans.length) {
            const orphanId = orphans.shift()!;
            this.reparentOrphan_(orphanId, orphans);
          }
        }

        /**
         * Re-parents the orphan to a layout that is not a member of
         * |otherOrphanIds|.
         * @param orphanId The id of the orphan to re-parent.
         * @param otherOrphanIds The list of ids of other orphans
         *     to ignore when re-parenting.
         */
        private reparentOrphan_(orphanId: string, otherOrphanIds: string[]):
            void {
          const layout = this.displayLayoutMap_.get(orphanId);
          assert(layout);
          if (orphanId === this.dragId && layout.parentId !== '') {
            this.setCalculatedDisplayBounds_(orphanId, this.dragBounds_);
            return;
          }
          const bounds = this.getCalculatedDisplayBounds(orphanId);

          // Find the closest parent.
          const newParentId =
              this.findClosest_(orphanId, bounds, otherOrphanIds);
          assert(newParentId !== '');
          layout.parentId = newParentId;

          // Find the closest edge.
          const parentBounds = this.getCalculatedDisplayBounds(newParentId);
          const layoutPosition =
              this.getLayoutPositionForBounds_(bounds, parentBounds);

          // Move from the nearest corner to the desired location and get the
          // delta.
          const cornerBounds = this.getCornerBounds_(bounds, parentBounds);
          const desiredPos =
              this.snapBounds_(bounds, newParentId, layoutPosition);
          const deltaPos = {
            x: desiredPos.x - cornerBounds.left,
            y: desiredPos.y - cornerBounds.top,
          };

          // Check for collisions.
          this.collideAndModifyDelta_(orphanId, cornerBounds, deltaPos);
          const desiredBounds = {
            left: cornerBounds.left + deltaPos.x,
            top: cornerBounds.top + deltaPos.y,
            width: bounds.width,
            height: bounds.height,
          };

          this.updateOffsetAndPosition_(desiredBounds, layoutPosition, layout);
        }

        /**
         * @param recurse Whether or not to include descendants of children.
         */
        private findChildren_(parentId: string, recurse: boolean): string[] {
          let children: string[] = [];
          this.displayLayoutMap_.forEach((value, key) => {
            const childId = key;
            if (childId !== parentId && value.parentId === parentId) {
              // Insert immediate children at the front of the array.
              children.unshift(childId);
              if (recurse) {
                // Descendants get added to the end of the list.
                children = children.concat(this.findChildren_(childId, true));
              }
            }
          });
          return children;
        }

        /**
         * Recursively calculates the absolute bounds of a display.
         * Caches the display bounds so that parent bounds are only calculated
         * once.
         */
        private calculateBounds_(id: string, width: number, height: number):
            void {
          let left: number;
          let top: number;
          const layout = this.displayLayoutMap_.get(id);
          if (this.mirroring || !layout || !layout.parentId) {
            left = -width / 2;
            top = -height / 2;
          } else {
            if (!this.calculatedBoundsMap_.has(layout.parentId)) {
              const pbounds = this.displayBoundsMap_.get(layout.parentId)!;
              this.calculateBounds_(
                  layout.parentId, pbounds.width, pbounds.height);
            }
            const parentBounds =
                this.getCalculatedDisplayBounds(layout.parentId);
            left = parentBounds.left;
            top = parentBounds.top;
            switch (layout.position) {
              case LayoutPosition.TOP:
                left += layout.offset;
                top -= height;
                break;
              case LayoutPosition.RIGHT:
                left += parentBounds.width;
                top += layout.offset;
                break;
              case LayoutPosition.BOTTOM:
                left += layout.offset;
                top += parentBounds.height;
                break;
              case LayoutPosition.LEFT:
                left -= width;
                top += layout.offset;
                break;
            }
          }
          const result = {
            left,
            top,
            width,
            height,
          };
          this.setCalculatedDisplayBounds_(id, result);
        }

        /**
         * Finds the display closest to |bounds| ignoring |ignoreIds|.
         */
        private findClosest_(
            displayId: string, bounds: Bounds, ignoreIds?: string[]): string {
          const x = bounds.left + bounds.width / 2;
          const y = bounds.top + bounds.height / 2;
          let closestId = '';
          let closestDelta2 = 0;
          const keys = this.calculatedBoundsMap_.keys();
          for (let iter = keys.next(); !iter.done; iter = keys.next()) {
            const otherId = iter.value;
            if (otherId === displayId) {
              continue;
            }
            if (ignoreIds && ignoreIds.includes(otherId)) {
              continue;
            }
            const {left, top, width, height} =
                this.getCalculatedDisplayBounds(otherId);
            if (x >= left && x < left + width && y >= top && y < top + height) {
              return otherId;
            }  // point is inside rect
            let dx: number;
            let dy: number;
            if (x < left) {
              dx = left - x;
            } else if (x > left + width) {
              dx = x - (left + width);
            } else {
              dx = 0;
            }
            if (y < top) {
              dy = top - y;
            } else if (y > top + height) {
              dy = y - (top + height);
            } else {
              dy = 0;
            }
            const delta2 = dx * dx + dy * dy;
            if (closestId === '' || delta2 < closestDelta2) {
              closestId = otherId;
              closestDelta2 = delta2;
            }
          }
          return closestId;
        }

        /**
         * Calculates the LayoutPosition for |bounds| relative to |parentId|.
         */
        private getLayoutPositionForBounds_(
            bounds: Bounds, parentBounds: Bounds): LayoutPosition {
          // Translate bounds from top-left to center.
          const x = bounds.left + bounds.width / 2;
          const y = bounds.top + bounds.height / 2;

          // Determine the distance from the new bounds to both of the near
          // edges.
          const {left, top, width, height} = parentBounds;

          // Signed deltas to the center.
          const dx = x - (left + width / 2);
          const dy = y - (top + height / 2);

          // Unsigned distance to each edge.
          const distx = Math.abs(dx) - width / 2;
          const disty = Math.abs(dy) - height / 2;

          if (distx > disty) {
            if (dx < 0) {
              return LayoutPosition.LEFT;
            }
            return LayoutPosition.RIGHT;
          } else {
            if (dy < 0) {
              return LayoutPosition.TOP;
            }
            return LayoutPosition.BOTTOM;
          }
        }

        /**
         * Modifies |bounds| to the position closest to it along the edge of
         * |parentId| specified by |layoutPosition|.
         */
        private snapBounds_(
            bounds: Bounds, parentId: string,
            layoutPosition: LayoutPosition): Position {
          const parentBounds = this.getCalculatedDisplayBounds(parentId);

          let x: number;
          if (layoutPosition === LayoutPosition.LEFT) {
            x = parentBounds.left - bounds.width;
          } else if (layoutPosition === LayoutPosition.RIGHT) {
            x = parentBounds.left + parentBounds.width;
          } else {
            x = this.snapToX_(bounds, parentBounds);
          }

          let y: number;
          if (layoutPosition === LayoutPosition.TOP) {
            y = parentBounds.top - bounds.height;
          } else if (layoutPosition === LayoutPosition.BOTTOM) {
            y = parentBounds.top + parentBounds.height;
          } else {
            y = this.snapToY_(bounds, parentBounds);
          }

          return {x, y};
        }

        /**
         * Snaps a horizontal value, see snapToEdge.
         * @param snapDistance Optionally provide to override the snap distance.
         *     0 means snap from any distance.
         */
        private snapToX_(
            newBounds: Bounds, parentBounds: Bounds,
            snapDistance?: number): number {
          return this.snapToEdge_(
              newBounds.left, newBounds.width, parentBounds.left,
              parentBounds.width, snapDistance);
        }

        /**
         * Snaps a vertical value, see snapToEdge.
         * @param snapDistance Optionally provide to override the snap distance.
         *     0 means snap from any distance.
         */
        private snapToY_(
            newBounds: Bounds, parentBounds: Bounds,
            snapDistance?: number): number {
          return this.snapToEdge_(
              newBounds.top, newBounds.height, parentBounds.top,
              parentBounds.height, snapDistance);
        }

        /**
         * Snaps the region [point, width] to [basePoint, baseWidth] if
         * the [point, width] is close enough to the base's edge.
         * @param snapDistance Provide to override the snap distance.
         *     0 means snap at any distance.
         * @return The moved point. Returns the point itself if it doesn't
         *     need to snap to the edge.
         */
        private snapToEdge_(
            point: number, width: number, basePoint: number, baseWidth: number,
            snapDistance?: number): number {
          // If the edge of the region is smaller than this, it will snap to the
          // base's edge.
          const SNAP_DISTANCE_PX = 16;
          const snapDist =
              (snapDistance !== undefined) ? snapDistance : SNAP_DISTANCE_PX;

          const startDiff = Math.abs(point - basePoint);
          const endDiff = Math.abs(point + width - (basePoint + baseWidth));
          // Prefer the closer one if both edges are close enough.
          if ((!snapDist || startDiff < snapDist) && startDiff < endDiff) {
            return basePoint;
          } else if (!snapDist || endDiff < snapDist) {
            return basePoint + baseWidth - width;
          }

          return point;
        }

        /**
         * Intersects |layout| with each other layout and reduces |deltaPos| to
         * avoid any collisions (or sets it to [0,0] if the display can not be
         * moved in the direction of |deltaPos|). Note: this assumes that
         * deltaPos is already 'snapped' to the parent edge, and therefore will
         * not collide with the parent, i.e. this is to prevent overlapping with
         * displays other than the parent.
         */
        private collideAndModifyDelta_(
            id: string, bounds: Bounds, deltaPos: Position): void {
          const keys = this.calculatedBoundsMap_.keys();
          const others = new Set(keys);
          others.delete(id);
          let checkCollisions = true;
          while (checkCollisions) {
            checkCollisions = false;
            const othersValues = others.values();
            for (let iter = othersValues.next(); !iter.done;
                 iter = othersValues.next()) {
              const otherId = iter.value;
              const otherBounds = this.getCalculatedDisplayBounds(otherId);
              if (this.collideWithBoundsAndModifyDelta_(
                      bounds, otherBounds, deltaPos)) {
                if (deltaPos.x === 0 && deltaPos.y === 0) {
                  return;
                }
                others.delete(otherId);
                checkCollisions = true;
                break;
              }
            }
          }
        }

        /**
         * Intersects |bounds| with |otherBounds|. If there is a collision,
         * modifies |deltaPos| to limit movement to a single axis and avoid the
         * collision and returns true. See note for |collideAndModifyDelta_|.
         */
        private collideWithBoundsAndModifyDelta_(
            bounds: Bounds, otherBounds: Bounds, deltaPos: Position): boolean {
          const newX = bounds.left + deltaPos.x;
          const newY = bounds.top + deltaPos.y;

          if ((newX + bounds.width <= otherBounds.left) ||
              (newX >= otherBounds.left + otherBounds.width) ||
              (newY + bounds.height <= otherBounds.top) ||
              (newY >= otherBounds.top + otherBounds.height)) {
            return false;
          }

          // |deltaPos| should already be restricted to X or Y. This shortens
          // the delta to stay outside the bounds, however it does not change
          // the sign of the delta, i.e. it does not "push" the point outside
          // the bounds if the point is already inside.
          if (Math.abs(deltaPos.x) > Math.abs(deltaPos.y)) {
            deltaPos.y = 0;
            let snapDeltaX: number;
            if (deltaPos.x > 0) {
              snapDeltaX =
                  Math.max(0, (otherBounds.left - bounds.width) - bounds.left);
            } else {
              snapDeltaX = Math.min(
                  0, (otherBounds.left + otherBounds.width) - bounds.left);
            }
            deltaPos.x = snapDeltaX;
          } else {
            deltaPos.x = 0;
            let snapDeltaY: number;
            if (deltaPos.y > 0) {
              snapDeltaY =
                  Math.min(0, (otherBounds.top - bounds.height) - bounds.top);
            } else if (deltaPos.y < 0) {
              snapDeltaY = Math.max(
                  0, (otherBounds.top + otherBounds.height) - bounds.top);
            } else {
              snapDeltaY = 0;
            }
            deltaPos.y = snapDeltaY;
          }

          return true;
        }

        /**
         * Updates the offset for |layout| from |bounds|.
         */
        private updateOffsetAndPosition_(
            bounds: Bounds, position: LayoutPosition,
            layout: DisplayLayout): void {
          layout.position = position;
          if (!layout.parentId) {
            layout.offset = 0;
            return;
          }

          // Offset is calculated from top or left edge.
          const parentBounds = this.getCalculatedDisplayBounds(layout.parentId);
          let offset: number;
          let minOffset: number;
          let maxOffset: number;
          if (position === LayoutPosition.LEFT ||
              position === LayoutPosition.RIGHT) {
            offset = bounds.top - parentBounds.top;
            minOffset = -bounds.height;
            maxOffset = parentBounds.height;
          } else {
            offset = bounds.left - parentBounds.left;
            minOffset = -bounds.width;
            maxOffset = parentBounds.width;
          }
          const MIN_OFFSET_OVERLAP = 50;
          minOffset += MIN_OFFSET_OVERLAP;
          maxOffset -= MIN_OFFSET_OVERLAP;
          layout.offset = Math.max(minOffset, Math.min(offset, maxOffset));

          // Update the calculated bounds to match the new offset.
          this.calculateBounds_(layout.id, bounds.width, bounds.height);
        }

        /**
         * Returns |bounds| translated to touch the closest corner of
         * |parentBounds|.
         */
        private getCornerBounds_(bounds: Bounds, parentBounds: Bounds): Bounds {
          let x: number;
          if (bounds.left > parentBounds.left + parentBounds.width / 2) {
            x = parentBounds.left + parentBounds.width;
          } else {
            x = parentBounds.left - bounds.width;
          }
          let y: number;
          if (bounds.top > parentBounds.top + parentBounds.height / 2) {
            y = parentBounds.top + parentBounds.height;
          } else {
            y = parentBounds.top - bounds.height;
          }
          return {
            left: x,
            top: y,
            width: bounds.width,
            height: bounds.height,
          };
        }

        /**
         * Highlights the edge of the div associated with |id| based on
         * |layoutPosition| and removes any other highlights. If
         * |layoutPosition| is undefined, removes all highlights.
         */
        private highlightEdge_(
            id: string, layoutPosition: LayoutPosition|undefined): void {
          for (let i = 0; i < this.layouts.length; ++i) {
            const layout = this.layouts[i];
            const highlight = (layout.id === id || layout.parentId === id) ?
                layoutPosition :
                undefined;
            const div = id ? this.shadowRoot!.getElementById(`_${id}`) :
                             this.shadowRoot!.getElementById(`_${layout.id}`);
            assert(div);
            div.classList.toggle(
                'highlight-right', highlight === LayoutPosition.RIGHT);
            div.classList.toggle(
                'highlight-left', highlight === LayoutPosition.LEFT);
            div.classList.toggle(
                'highlight-top', highlight === LayoutPosition.TOP);
            div.classList.toggle(
                'highlight-bottom', highlight === LayoutPosition.BOTTOM);
          }
        }
      }

      return LayoutMixinInternal;
    });
