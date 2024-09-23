// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PointF, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import type {Word} from './text.mojom-webui.js';
import {WritingDirection} from './text.mojom-webui.js';

// Percentage of image size to add to the word bounding boxes hit box.
const BOUNDING_BOX_HIT_MARGIN_PERCENT = 0.02;

/**
 * Helper function that implements hit testing for words. A hit is used to
 * determine the users intent when their mouse is not on a specific word.
 *
 * @param words The list of words to check for a hit with
 * @param target The point normalized to image coordinates to check for a hit.
 * @return The best hit, null if no matches are found.
 */
export function bestHit(words: Word[], target: PointF): Word|null {
  if (words.length === 0) {
    return null;
  }

  return bestHorizontallyExtendedHit(words, target) ||
      bestVerticalHit(words, target);
}

// Finds the closes hit when extending alal word hit boxes horizontally.
function bestHorizontallyExtendedHit(words: Word[], target: PointF): Word|null {
  const hits: Word[] = [];
  for (const word of words) {
    const hit = horizontallyExtendedHit(word, target);
    if (hit) {
      hits.push(hit);
    }
  }

  return getBestHit(hits, target);
}

// Finds the best hit that is vertically close.
function bestVerticalHit(words: Word[], target: PointF): Word|null {
  const closestProjectedDistances: Word[] =
      getShortedProjectedDistanceHits(words, target);

  return getBestHit(closestProjectedDistances, target);
}

function horizontallyExtendedHit(word: Word, target: PointF): Word|null {
  const position = rotateCoordinate(word, target);
  const boundingBox = boundingBoxWithMargin(word);

  if (boxContainsTarget(boundingBox, position)) {
    return word;
  }

  if (isInExtension(position, boundingBox, word.writingDirection)) {
    return word;
  }

  return null;
}

// Finds the hit closest the the target.
function getBestHit(hits: Word[], target: PointF): Word|null {
  let closestHit = null;
  let closestDistance = Number.MAX_SAFE_INTEGER;

  for (const hit of hits) {
    const wordBoundingBox = hit.geometry?.boundingBox.box;
    if (!wordBoundingBox) {
      continue;
    }

    const distX = target.x - wordBoundingBox.x;
    const distY = target.y - wordBoundingBox.y;
    const totalDistance = distX * distX + distY * distY;
    if (totalDistance < closestDistance) {
      closestHit = hit;
      closestDistance = totalDistance;
    }
  }

  return closestHit;
}

// Rotates the target coordinates to be in relation to the word rotation.
function rotateCoordinate(word: Word, target: PointF): PointF {
  const wordBoundingBox = word.geometry?.boundingBox;
  if (!wordBoundingBox) {
    return {x: 0, y: 0};
  }
  // Translate to origin 0,0.
  const translatedX = target.x - wordBoundingBox.box.x;
  const translatedY = target.y - wordBoundingBox.box.y;

  // Rotate clockwise around origin.
  const rotatedX = translatedX * Math.cos(wordBoundingBox.rotation) +
      translatedY * Math.sin(wordBoundingBox.rotation);
  const rotatedY = -translatedX * Math.sin(wordBoundingBox.rotation) +
      translatedY * Math.cos(wordBoundingBox.rotation);

  // Translate the coordinate back.
  const finalX = rotatedX + wordBoundingBox.box.x;
  const finalY = rotatedY + wordBoundingBox.box.y;

  return {x: finalX, y: finalY};
}

// Creates a bounding box for the word, with additional BOUNDING_BOX_HIT_MARGIN.
function boundingBoxWithMargin(word: Word): RectF {
  const wordBoundingBox = word.geometry?.boundingBox.box;
  if (!wordBoundingBox) {
    return {x: 0, y: 0, width: 0, height: 0};
  }

  return {
    x: wordBoundingBox.x,
    y: wordBoundingBox.y,
    width: wordBoundingBox.width + BOUNDING_BOX_HIT_MARGIN_PERCENT,
    height: wordBoundingBox.height + BOUNDING_BOX_HIT_MARGIN_PERCENT,
  };
}

/** @return True if target is within the box. */
function boxContainsTarget(box: RectF, target: PointF): boolean {
  const boxMinX = box.x - (box.width / 2);
  const boxMaxX = box.x + (box.width / 2);
  const boxMinY = box.y - (box.height / 2);
  const boxMaxY = box.y + (box.height / 2);

  return target.x >= boxMinX && target.x <= boxMaxX && target.y >= boxMinY &&
      target.y <= boxMaxY;
}

/**
 * Determines if the position is within the giving bounding box, if the bounding
 * box expanded horizontally infinetely.
 * @returns True if the position is within the horizontal expansion of the word.
 */
function isInExtension(
    position: PointF, boundingBox: RectF,
    writingDirection: WritingDirection|null): boolean {
  const boxLeft = boundingBox.x - (boundingBox.width / 2);
  const boxRight = boundingBox.x + (boundingBox.width / 2);
  const boxTop = boundingBox.y - (boundingBox.height / 2);
  const boxBottom = boundingBox.y + (boundingBox.height / 2);

  if (writingDirection === WritingDirection.kTopToBottom) {
    const isAboveBox = position.x > boxLeft && position.x < boxRight &&
        position.y <= boundingBox.y;
    const isBelowBox = position.x > boxLeft && position.x < boxRight &&
        position.y >= boundingBox.y;
    return isAboveBox || isBelowBox;
  }

  // If writingDirection is null, assume left to right.
  const isLeftOfBox = position.y > boxTop && position.y < boxBottom &&
      position.x <= boundingBox.x;
  const isRightOfBox = position.y > boxTop && position.y < boxBottom &&
      position.x >= boundingBox.x;
  return isLeftOfBox || isRightOfBox;
}

/**
 * Gets the hits with the shortest projected distance as defined by
 * getProjectedDistance().
 * @returns The list of hits with the shortest distance to the target.
 */
function getShortedProjectedDistanceHits(
    words: Word[], target: PointF): Word[] {
  let closestHits: Word[] = [];
  let closestProjectedDistance = Number.MAX_SAFE_INTEGER;

  for (const word of words) {
    const projectedDistance = getProjectedDistance(word, target);
    if (!projectedDistance) {
      continue;
    }

    if (Math.abs(closestProjectedDistance - projectedDistance) <=
        BOUNDING_BOX_HIT_MARGIN_PERCENT) {
      // This porjected distance is close enough within the hit margin, so add
      // to closest hits.
      closestHits.push(word);
    } else if (projectedDistance <= closestProjectedDistance) {
      // Sizable new closest projected distance, so reset closest hits.
      closestHits = [word];
      closestProjectedDistance = projectedDistance;
    }
  }
  return closestHits;
}


/**
 * Calculates the projected distance from the target to the word. The projected
 * distance is the "vertical" distance from the target to the top/bottom of the
 * word. For top to bottom reading languages, the projected distance is the
 * distance from the right/left of the word.
 * @returns The projected distance from the target to the word. Null if the word
 *          does not have a bounding box.
 */
function getProjectedDistance(word: Word, target: PointF): number|null {
  const rotatedTarget = rotateCoordinate(word, target);
  const boundingBox = word.geometry?.boundingBox.box;
  if (!boundingBox) {
    return null;
  }

  const deltaX = Math.max(
      Math.abs(boundingBox.x - rotatedTarget.x) - boundingBox.width / 2, 0);
  const deltaY = Math.max(
      Math.abs(boundingBox.y - rotatedTarget.y) - boundingBox.height / 2, 0);
  return word.writingDirection === WritingDirection.kTopToBottom ? deltaX :
                                                                   deltaY;
}
