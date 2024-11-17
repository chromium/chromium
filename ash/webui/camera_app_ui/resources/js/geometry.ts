// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

/**
 * Size of Rectangle.
 */
export class Size {
  constructor(readonly width: number, readonly height: number) {}
}

/**
 * 2D point.
 */
export class Point {
  constructor(readonly x: number, readonly y: number) {}
}

const ORIGIN = new Point(0, 0);

/**
 * 2D vector.
 */
export class Vector {
  constructor(readonly x: number, readonly y: number) {}

  length(): number {
    return Math.hypot(this.x, this.y);
  }

  /**
   * Square distance.
   */
  length2(): number {
    return this.x * this.x + this.y * this.y;
  }

  add(v: Vector): Vector {
    return new Vector(this.x + v.x, this.y + v.y);
  }

  minus(v: Vector): Vector {
    return new Vector(this.x - v.x, this.y - v.y);
  }

  /**
   * Dot product.
   */
  dot(v: Vector): number {
    return this.x * v.x + this.y * v.y;
  }

  /**
   * Cross product.
   */
  cross(v: Vector): number {
    return this.x * v.y - this.y * v.x;
  }

  multiply(n: number): Vector {
    return new Vector(this.x * n, this.y * n);
  }

  /**
   * @return Angle required to rotate from this vector to |v|.
   *     Positive/negative sign represent rotating in (counter-)clockwise
   *     direction.
   */
  rotation(v: Vector): number {
    const cross = this.cross(v);
    const dot = this.dot(v);
    return Math.atan2(cross, dot);
  }

  /**
   * The rotation angle for setting |CSSRotate|.
   */
  cssRotateAngle(): number {
    return ROTATE_START_AXIS.rotation(this);
  }

  /**
   * Unit direction vector.
   */
  direction(): Vector {
    const length = this.length();
    return new Vector(this.x / length, this.y / length);
  }

  /**
   * Unit normal vector n in direction that the |this| x |n| is positive.
   */
  normal(): Vector {
    const length = this.length();
    return new Vector(-this.y / length, this.x / length);
  }

  /**
   * @return Returns the vector point to reverse direction.
   */
  reverse(): Vector {
    return new Vector(-this.x, -this.y);
  }

  point(): Point {
    return new Point(this.x, this.y);
  }
}

/**
 * @return Vector points from |start| to |end|.
 */
export function vectorFromPoints(end: Point, start = ORIGIN): Vector {
  return new Vector(end.x - start.x, end.y - start.y);
}

/**
 * Start axis of |CSSRotate|.
 */
const ROTATE_START_AXIS = new Vector(1, 0);

/**
 * Vector with polar representation.
 */
export class PolarVector extends Vector {
  /**
   * @param angle Counter closewise angle start from (1, 0) in rad.
   */
  constructor(angle: number, length: number) {
    super(Math.cos(angle) * length, Math.sin(angle) * length);
  }
}

/**
 * Line formed by a point and a vector.
 */
export class Line {
  constructor(readonly point: Point, readonly direction: Vector) {}

  /**
   * Move parallelly in |this.direction.normal()| direction with specified
   * distance.
   */
  moveParallel(distance: number): Line {
    const newPt = vectorFromPoints(this.point)
                      .add(this.direction.normal().multiply(distance))
                      .point();
    return new Line(newPt, this.direction);
  }

  /**
   * @return Intersection with another line.
   */
  intersect(line: Line): Point|null {
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/geometry/float_polygon.cc;drc=977ef9c6935c51f8e4fdb1b0a81fdba23bf5563c;l=214
    const det = this.direction.cross(line.direction);
    if (det === 0) {
      return null;
    }
    const ptDelta = vectorFromPoints(this.point, line.point);
    const u = line.direction.cross(ptDelta) / det;
    return vectorFromPoints(this.point).add(this.direction.multiply(u)).point();
  }

  /**
   * @return If the |pt| fall in the half plane in normal vector
   *     direction divided by this line.
   */
  isInward(pt: Point): boolean {
    const v = vectorFromPoints(pt, this.point);
    return this.direction.cross(v) >= 0;
  }
}

/**
 * Bounding box position at origin.
 */
export class Box {
  constructor(readonly size: Size) {}

  inside(pt: Point): boolean {
    return 0 <= pt.x && pt.x <= this.size.width && 0 <= pt.y &&
        pt.y <= this.size.height;
  }

  /**
   * Calculates intersection with a ray formed by a start point(inside box) and
   * a pointing direction.
   *
   * @param pt Start point of the ray.
   * @param dir Direction of the ray.
   * @return The intersection point.
   */
  rayIntersect(pt: Point, dir: Vector): Point {
    assert(
        0 <= pt.x && pt.x <= this.size.width && 0 <= pt.y &&
        pt.y <= this.size.height);
    assert(dir.x !== 0 || dir.y !== 0);

    if (dir.x === 0) {
      // Horizontal ray should intersect one of the vertical sides.
      return new Point(pt.x, dir.y > 0 ? this.size.height : 0);
    } else {
      // Check the wall hit when moving in x component direction.
      const toWall = (dir.x > 0 ? this.size.width - pt.x : pt.x);
      const hitY = pt.y + dir.y * toWall / Math.abs(dir.x);
      if (0 <= hitY && hitY <= this.size.height) {
        return new Point(dir.x > 0 ? this.size.width : 0, hitY);
      }
    }
    if (dir.y === 0) {
      // Vertical ray should intersect one of the horizontal sides.
      return new Point(dir.x > 0 ? this.size.width : 0, pt.y);
    } else {
      // Check the wall hit when moving in y component direction.
      const toWall = (dir.y > 0 ? this.size.height - pt.y : pt.y);
      const hitX = pt.x + dir.x * toWall / Math.abs(dir.y);
      assert(0 <= hitX && hitX <= this.size.width);
      return new Point(hitX, dir.y > 0 ? this.size.height : 0);
    }
  }

  /**
   * Calculates intersection with segment.
   *
   * @return Intersections with segment formed by |pt| and |pt2|.
   */
  segmentIntersect(pt: Point, pt2: Point): Point[] {
    /**
     * Intersection point of two line segments in 2 dimensions:
     * http://paulbourke.net/geometry/pointlineplane/.
     *
     * @return Intersection of segment pt1, pt2 and segment pt3, pt4.
     *     Returns null for no intersection between two segment.
     */
    function intersect(pt1: Point, pt2: Point, pt3: Point, pt4: Point): Point|
        null {
      const uDenom =
          (pt4.y - pt3.y) * (pt2.x - pt1.x) - (pt4.x - pt3.x) * (pt2.y - pt1.y);
      if (uDenom === 0) {
        return null;
      }
      const ua = ((pt4.x - pt3.x) * (pt1.y - pt3.y) -
                  (pt4.y - pt3.y) * (pt1.x - pt3.x)) /
          uDenom;
      const ub = ((pt2.x - pt1.x) * (pt1.y - pt3.y) -
                  (pt2.y - pt1.y) * (pt1.x - pt3.x)) /
          uDenom;
      if (0 <= ua && ua <= 1 && 0 <= ub && ub <= 1) {
        return new Point(
            pt1.x + ua * (pt2.x - pt1.x), pt1.y + ua * (pt2.y - pt1.y));
      }
      return null;
    }

    const cornRd = new Point(this.size.width, this.size.height);
    const cornLd = new Point(0, this.size.height);
    const cornLu = new Point(0, 0);
    const cornRu = new Point(this.size.width, 0);
    const segs: Array<[Point, Point]> = [
      [cornRu, cornLu],
      [cornRd, cornRu],
      [cornLu, cornLd],
      [cornLd, cornRd],
    ];

    const intersectPts = [];
    for (const seg of segs) {
      const intersectPt = intersect(pt, pt2, seg[0], seg[1]);
      if (intersectPt !== null) {
        intersectPts.push(intersectPt);
      }
    }

    return intersectPts;
  }
}
