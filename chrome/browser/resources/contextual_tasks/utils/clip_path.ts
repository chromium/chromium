// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Rect} from '../post_message_handler.js';

/**
 * Returns a clip path for the thread frame to clip out the portion of the
 * composebox that is NOT covered by any AIM elements. The function finds the
 * difference between the composebox bounds and the occluders, resulting in only
 * the part of the composebox page that is clickable to the user.
 *
 * @param composeboxBounds The bounds of the composebox relative to the
 *     viewport.
 * @param occluders The bounds of the occluders relative to the viewport.
 * @param padding An additional buffer in pixels to add to the bounds of each
 *     occluder.
 * @return The clip path for the thread frame the cuts out a hold in the
 *     threadframe where the composebox is.
 */
export function getNonOccludedClipPath(
    composeboxBounds: Rect|null, occluders: Rect[], padding: number): string {
  // If no composebox bounds, return an empty clip path.
  if (!composeboxBounds) {
    return '';
  }

  const hole = {
    top: composeboxBounds.top,
    bottom: composeboxBounds.bottom,
    left: composeboxBounds.left,
    right: composeboxBounds.right,
    width: composeboxBounds.width,
  };

  // 2. Vertical Clipping
  // Firstly, handle cases where the occluder spans the full width of the
  // composebox. This handles cases where the composebox might be fully
  // occluded. Start with the full height of the composebox as the hole, and
  // chip away from the Top & Bottom.
  let holeTop = hole.top;
  let holeBottom = hole.bottom;
  const remainingOccluders: Rect[] = [];

  for (const occluder of occluders) {
    // Apply buffer to occluder. This helps to account for cases where the
    // occluder is not a perfect rectangle, or the bounding box does not match
    // the actual pixels rendered.
    const paddedOccluder = {
      top: occluder.top - padding,
      bottom: occluder.bottom + padding,
      left: occluder.left - padding,
      right: occluder.right + padding,
      width: occluder.width + (padding * 2),
      height: occluder.height + (padding * 2),
    };

    if (paddedOccluder.width >= hole.width) {
      // Clip the top of the current hole if the occluder overlaps the top
      if (paddedOccluder.bottom > holeTop && paddedOccluder.bottom < holeBottom) {
        // Move the top of the hold down to where the occluder bottom is.
        holeTop = paddedOccluder.bottom;
      }
      // Clip the bottom of the current hole if the occluder overlaps the bottom
      if (paddedOccluder.top < holeBottom && paddedOccluder.top > holeTop) {
        // Move the bottom of the hole up to where the occluder top is.
        holeBottom = paddedOccluder.top;
      }
      // If the occluder covers the entire hole, set the hole to be zero height.
      if (paddedOccluder.top <= holeTop && paddedOccluder.bottom >= holeBottom) {
        holeTop = holeBottom;
      }
    } else {
      remainingOccluders.push(paddedOccluder);
    }
  }

  // If fully vertically clipped, close the hole so no part of the composebox
  // is clickable.
  if (holeTop >= holeBottom) {
    return 'clip-path: none;';
  }

  // 3. Horizontal Segmentation
  // Handle cases where a non-full-width occluder might be floating in the
  // middle or on the edge of the composebox.

  // Start with one full segment and slice it up
  let visibleSegments: Array<{start: number, end: number}> =
      [{start: hole.left, end: hole.right}];

  // Loop through all the remaining occluders and clip them from the visible
  // segments. Note: these are already padded.
  for (const occ of remainingOccluders) {
    // Check if this occluder overlaps the vertically trimmed hole
    if (occ.right > hole.left && occ.left < hole.right &&
        occ.bottom > holeTop && occ.top < holeBottom) {
      const newSegments: Array<{start: number, end: number}> = [];
      for (const seg of visibleSegments) {
        // Case A: Occluder fully covers segment -> Drop the visible segment
        if (occ.left <= seg.start && occ.right >= seg.end) {
          continue;
        }

        // Case B: No overlap -> Keep this visible segment
        if (occ.right <= seg.start || occ.left >= seg.end) {
          newSegments.push(seg);
          continue;
        }

        // Case C: Partial Overlap -> Split the visible segment into two to clip
        // around the occluder.
        if (occ.left > seg.start) {
          newSegments.push({start: seg.start, end: occ.left});
        }
        if (occ.right < seg.end) {
          newSegments.push({start: occ.right, end: seg.end});
        }
      }
      visibleSegments = newSegments;
    }
  }

  // 4. Polygon Construction
  // Finally, now that visibleSegments represents the portions of the composebox
  // that are not occluded and should be clickable, construct the clip path to
  // cut a hole in the <webview> for these visible segments.
  // Sort segments Left-to-Right to prevent crossing lines
  visibleSegments.sort((a, b) => a.start - b.start);

  // Start Outer Box (Clockwise)
  let path = `0% 0%, 100% 0%, 100% 100%, 0% 100%`;

  // Enter Canal to the start of the hole (Bottom of the search box)
  // A canal is a long thin rectangle that runs from the edge of the screen to
  // the start of the hole. Since a clip-path needs to be one continuous line,
  // the canal is drawn first, then the holes, then the canal again to close it
  // off.
  path += `, 0px ${holeBottom}px`;

  // Draw Holes (Counter-Clockwise: BL -> BR -> TR -> TL)
  // This winding order is critical for the "subtraction" to work
  for (const seg of visibleSegments) {
    path += ',' +
        ` ${seg.start}px ${holeBottom}px,` +
        ` ${seg.end}px ${holeBottom}px,` +
        ` ${seg.end}px ${holeTop}px,` +
        ` ${seg.start}px ${holeTop}px,` +
        ` ${seg.start}px ${holeBottom}px`;
  }

  // Exit Canal and Close
  path += `, 0px ${holeBottom}px, 0% 100%`;

  // Apply
  return `clip-path: polygon(${path});`;
}
