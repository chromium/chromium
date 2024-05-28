// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import type {Word} from './text.mojom-webui.js';

// A point in the normalized coordinate plane.
interface Vertex {
  x: number;
  y: number;
}

// A bounding box, in normalized coordinates.
interface BoundingBox {
  left: number;
  right: number;
  top: number;
  bottom: number;
}

export enum ClippingEdge {
  LEFT,
  RIGHT,
  TOP,
  BOTTOM,
}

export interface WordsInRegionResult {
  // The index of the first word in the selection, or -1 if no words found.
  startIndex: number;
  // The index of the last word in the selection, or -1 if no words found.
  endIndex: number;
  // The intersection over union between the bounding boxes of the selected
  // words and the selection region.
  iou: number;
}

// Searches for words intersecting the selection region. Since coordinates are
// normalized, the image bounds are required to correctly handle rotations.
export function findWordsInRegion(
    words: Word[], selectionRegion: CenterRotatedBox,
    imageBounds: DOMRect): WordsInRegionResult {
  let intersectionArea = 0;
  let unionArea = areaOfPolygon(toPolygon(selectionRegion, imageBounds));
  let startIndex = -1;
  let endIndex = -1;
  const selectionBounds = {
    left: selectionRegion.box.x - selectionRegion.box.width / 2,
    right: selectionRegion.box.x + selectionRegion.box.width / 2,
    top: selectionRegion.box.y - selectionRegion.box.height / 2,
    bottom: selectionRegion.box.y + selectionRegion.box.height / 2,
  };
  const wordPolygons =
      words.map((word) => toPolygon(word.geometry!.boundingBox, imageBounds));
  const clippedWords =
      wordPolygons.map((wordPolygon) => clip(wordPolygon, selectionBounds));

  for (let i = 0; i < words.length; i++) {
    const wordPolygon = wordPolygons[i];
    const clippedWord = clippedWords[i];
    const wordIntersectionArea = areaOfPolygon(clippedWord);
    if (wordIntersectionArea > 0) {
      if (startIndex === -1) {
        startIndex = endIndex = i;
      } else {
        startIndex = Math.min(startIndex, i);
        endIndex = Math.max(endIndex, i);
      }
      intersectionArea += wordIntersectionArea;
      unionArea += areaOfPolygon(wordPolygon) - wordIntersectionArea;
    }
  }
  const iou = intersectionArea / unionArea;
  return {iou, startIndex, endIndex};
}

// Converts a CenterRotatedBox to an array of vertices representing a polygon.
export function toPolygon(
    box: CenterRotatedBox, imageBounds: DOMRect): Vertex[] {
  const polygon: Vertex[] = [];
  const center = {x: box.box.x, y: box.box.y};
  const left = box.box.x - box.box.width / 2;
  const right = box.box.x + box.box.width / 2;
  const top = box.box.y - box.box.height / 2;
  const bottom = box.box.y + box.box.height / 2;
  polygon.push(rotate(center, box.rotation, {x: left, y: top}, imageBounds));
  polygon.push(rotate(center, box.rotation, {x: right, y: top}, imageBounds));
  polygon.push(
      rotate(center, box.rotation, {x: right, y: bottom}, imageBounds));
  polygon.push(rotate(center, box.rotation, {x: left, y: bottom}, imageBounds));
  return polygon;
}

// Calculates the area of a polygon using the shoelace formula.
export function areaOfPolygon(polygon: Vertex[]): number {
  let area = 0;
  for (let i = 0; i < polygon.length; i++) {
    if (i < polygon.length - 1) {
      area += polygon[i].x * polygon[i + 1].y - polygon[i + 1].x * polygon[i].y;
    } else {
      area += polygon[i].x * polygon[0].y - polygon[0].x * polygon[i].y;
    }
  }
  return 0.5 * Math.abs(area);
}

// Returns the result of rotating the target vertex about the anchor vertex.
// All vertices are defined in normalized coordinates, so the image bounds
// are used to convert to absolute coordinates while rotating.
export function rotate(
    anchor: Vertex, angleRadian: number, target: Vertex,
    imageBounds: DOMRect): Vertex {
  const anchorXAbs = anchor.x * imageBounds.width;
  const anchorYAbs = anchor.y * imageBounds.height;
  const targetXAbs = target.x * imageBounds.width;
  const targetYAbs = target.y * imageBounds.height;

  const cosAngle = Math.cos(angleRadian);
  const sinAngle = Math.sin(angleRadian);
  const deltaX = targetXAbs - anchorXAbs;
  const deltaY = targetYAbs - anchorYAbs;
  const rotatedDeltaX = deltaX * cosAngle - deltaY * sinAngle;
  const rotatedDeltaY = deltaY * cosAngle + deltaX * sinAngle;
  return {
    x: (anchorXAbs + rotatedDeltaX) / imageBounds.width,
    y: (anchorYAbs + rotatedDeltaY) / imageBounds.height,
  };
}

// Clips the polygon to the selection bounds. As the selection
// region is an axis-aligned rectangle, a simplified case of the
// Sutherland-Hodgman algorithm may be used.
export function clip(polygon: Vertex[], selectionBounds: BoundingBox) {
  polygon = clipEdge(polygon, selectionBounds, ClippingEdge.TOP);
  polygon = clipEdge(polygon, selectionBounds, ClippingEdge.LEFT);
  polygon = clipEdge(polygon, selectionBounds, ClippingEdge.BOTTOM);
  polygon = clipEdge(polygon, selectionBounds, ClippingEdge.RIGHT);
  return polygon;
}

// Clips the polygon to a single edge of the selection bounds.
function clipEdge(
    polygon: Vertex[], selectionBounds: BoundingBox,
    edge: ClippingEdge): Vertex[] {
  const clippedPolygon: Vertex[] = [];
  if (polygon.length === 0) {
    return clippedPolygon;
  }
  let previous: Vertex;
  let next = polygon[polygon.length - 1];
  let isPreviousInside: boolean;
  let isNextInside = isInsideEdge(next, selectionBounds, edge);
  for (let i = 0; i < polygon.length; i++) {
    previous = next;
    next = polygon[i];
    isPreviousInside = isNextInside;
    isNextInside = isInsideEdge(next, selectionBounds, edge);
    if (isPreviousInside !== isNextInside) {
      clippedPolygon.push(
          intersectionWithEdge(previous, next, selectionBounds, edge));
    }
    if (isNextInside) {
      clippedPolygon.push(next);
    }
  }
  return clippedPolygon;
}

// Returns whether the given vertex is inside the specified edge of the
// selection bounds.
export function isInsideEdge(
    vertex: Vertex, selectionBounds: BoundingBox, edge: ClippingEdge) {
  switch (edge) {
    case ClippingEdge.LEFT:
      return vertex.x >= selectionBounds.left;
    case ClippingEdge.RIGHT:
      return vertex.x <= selectionBounds.right;
    case ClippingEdge.TOP:
      return vertex.y >= selectionBounds.top;
    case ClippingEdge.BOTTOM:
      return vertex.y <= selectionBounds.bottom;
  }
}

// Given two vertices defining a line, returns the intersection of that line
// with the specified edge of the selection bounds.
export function intersectionWithEdge(
    v0: Vertex, v1: Vertex, selectionBounds: BoundingBox,
    edge: ClippingEdge): Vertex {
  switch (edge) {
    case ClippingEdge.LEFT:
      return {
        x: selectionBounds.left,
        y: v0.y + (v1.y - v0.y) * (selectionBounds.left - v0.x) / (v1.x - v0.x),
      };
    case ClippingEdge.RIGHT:
      return {
        x: selectionBounds.right,
        y: v0.y +
            (v1.y - v0.y) * (selectionBounds.right - v0.x) / (v1.x - v0.x),
      };
    case ClippingEdge.TOP:
      return {
        x: v0.x + (v1.x - v0.x) * (selectionBounds.top - v0.y) / (v1.y - v0.y),
        y: selectionBounds.top,
      };
    case ClippingEdge.BOTTOM:
      return {
        x: v0.x +
            (v1.x - v0.x) * (selectionBounds.bottom - v0.y) / (v1.y - v0.y),
        y: selectionBounds.bottom,
      };
  }
}
