# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Optimizes SVG PathItem sequences (replicates SVGO convertPathData)."""

from collections.abc import Sequence
import dataclasses
import math
from typing import Optional

from svg_resizer import PathItem, js_num_str  # pytype: disable=import-error

PRECISION = 3
ERROR = 1e-3
ARC_THRESHOLD = 2.5
ARC_TOLERANCE = 0.5


@dataclasses.dataclass
class Circle:
    """Represents a fitted circle for cubic Bezier to arc conversion."""

    center: list[float]
    radius: float


def to_fixed(num: float, prec: int) -> float:
    """Rounds half towards +Infinity at the given decimal precision."""
    pow10 = 10**prec
    return math.floor(num * pow10 + 0.5) / pow10


def remove_leading_zero(value: float) -> str:
    """Strips the leading zero from fractional values in (-1, 1)."""
    str_value = js_num_str(value)
    if 0 < value < 1 and str_value.startswith('0'):
        return str_value[1:]
    if -1 < value < 0 and len(str_value) > 1 and str_value[1] == '0':
        return str_value[0] + str_value[2:]
    return str_value


def cleanup_out_data(data: Sequence[float]) -> str:
    """Formats a sequence of numeric path arguments as a compact SVG string."""
    out = ''
    prev = 0.0
    for i, item in enumerate(data):
        delimiter = '' if i == 0 else ' '
        item_str = remove_leading_zero(item)
        if delimiter and (
            item < 0
            or (item_str.startswith('.') and not float(prev).is_integer())
        ):
            delimiter = ''
        prev = item
        out += delimiter + item_str
    return out


def strong_round(data: list[float]) -> list[float]:
    """Rounds numbers in-place to PRECISION or PRECISION-1 when within ERROR."""
    for i in range(len(data) - 1, -1, -1):
        fixed = to_fixed(data[i], PRECISION)
        if fixed != data[i]:
            rounded = to_fixed(data[i], PRECISION - 1)
            data[i] = (
                fixed
                if to_fixed(abs(rounded - data[i]), PRECISION + 1) >= ERROR
                else rounded
            )
    return data


def get_intersection(coords: Sequence[float]) -> Optional[list[float]]:
    """Finds the intersection of two lines defined by 8 coordinates."""
    a1 = coords[1] - coords[3]
    b1 = coords[2] - coords[0]
    c1 = coords[0] * coords[3] - coords[2] * coords[1]
    a2 = coords[5] - coords[7]
    b2 = coords[6] - coords[4]
    c2 = coords[4] * coords[7] - coords[5] * coords[6]
    denom = a1 * b2 - a2 * b1
    if not denom:
        return None
    cross = [(b1 * c2 - b2 * c1) / denom, (a1 * c2 - a2 * c1) / -denom]
    if math.isfinite(cross[0]) and math.isfinite(cross[1]):
        return cross
    return None


def is_convex(data: Sequence[float]) -> bool:
    """Returns True if the relative cubic Bezier control polygon is convex."""
    center = get_intersection(
        [0, 0, data[2], data[3], data[0], data[1], data[4], data[5]]
    )
    return (
        center is not None
        and ((data[2] < center[0]) == (center[0] < 0))
        and ((data[3] < center[1]) == (center[1] < 0))
        and ((data[4] < center[0]) == (center[0] < data[0]))
        and ((data[5] < center[1]) == (center[1] < data[1]))
    )


def is_curve_straight_line(data: Sequence[float]) -> bool:
    """Returns True if all control points of a curve lie on a straight line."""
    i = len(data) - 2
    if i <= 1:
        return False
    a = -data[i + 1]
    b = data[i]
    denom = a * a + b * b
    if denom == 0 or not math.isfinite(1.0 / denom):
        return False
    d = 1.0 / denom
    i -= 2
    while i >= 0:
        if math.sqrt(((a * data[i] + b * data[i + 1]) ** 2) * d) > ERROR:
            return False
        i -= 2
    return True


def calculate_sagitta(data: Sequence[float]) -> Optional[float]:
    """Calculates the sagitta of an arc if circular and non-large."""
    if data[3] == 1:
        return None
    rx, ry = data[0], data[1]
    if abs(rx - ry) > ERROR:
        return None
    chord = math.sqrt(data[5] ** 2 + data[6] ** 2)
    if chord > rx * 2:
        return None
    radicand = rx**2 - 0.25 * (chord**2)
    if radicand < 0:
        return None
    return rx - math.sqrt(radicand)


def make_longhand(item: PathItem, data: Sequence[float]) -> PathItem:
    """Converts a smooth shorthand cubic curve (s) into longhand (c)."""
    if item.command == 's':
        item.command = 'c'
    item.args.insert(0, data[-1] - data[-3])
    item.args.insert(0, data[-2] - data[-4])
    return item


def get_distance(p1: Sequence[float], p2: Sequence[float]) -> float:
    """Returns the Euclidean distance between two 2D points."""
    return math.sqrt((p1[0] - p2[0]) ** 2 + (p1[1] - p2[1]) ** 2)


def get_cubic_bezier_point(curve: Sequence[float], t: float) -> list[float]:
    """Evaluates a relative cubic Bezier curve starting at (0, 0) at t."""
    sqr_t = t * t
    cub_t = sqr_t * t
    mt = 1 - t
    sqr_mt = mt * mt
    return [
        3 * sqr_mt * t * curve[0]
        + 3 * mt * sqr_t * curve[2]
        + cub_t * curve[4],
        3 * sqr_mt * t * curve[1]
        + 3 * mt * sqr_t * curve[3]
        + cub_t * curve[5],
    ]


def find_circle(curve: Sequence[float]) -> Optional[Circle]:
    """Fits a circle to a relative cubic Bezier curve if within tolerance."""
    mid_point = get_cubic_bezier_point(curve, 0.5)
    m1 = [mid_point[0] / 2, mid_point[1] / 2]
    m2 = [(mid_point[0] + curve[4]) / 2, (mid_point[1] + curve[5]) / 2]
    center = get_intersection(
        [
            m1[0],
            m1[1],
            m1[0] + m1[1],
            m1[1] - m1[0],
            m2[0],
            m2[1],
            m2[0] + (m2[1] - mid_point[1]),
            m2[1] - (m2[0] - mid_point[0]),
        ]
    )
    if center is None:
        return None
    radius = get_distance([0.0, 0.0], center)
    tolerance = min(ARC_THRESHOLD * ERROR, (ARC_TOLERANCE * radius) / 100)
    if radius < 1e15 and all(
        abs(get_distance(get_cubic_bezier_point(curve, pt), center) - radius)
        <= tolerance
        for pt in (0.25, 0.75)
    ):
        return Circle(center=center, radius=radius)
    return None


def is_arc(curve: Sequence[float], circle: Circle) -> bool:
    """Checks whether a cubic Bezier lies on the given circle."""
    tolerance = min(
        ARC_THRESHOLD * ERROR, (ARC_TOLERANCE * circle.radius) / 100
    )
    return all(
        abs(
            get_distance(get_cubic_bezier_point(curve, pt), circle.center)
            - circle.radius
        )
        <= tolerance
        for pt in (0.0, 0.25, 0.5, 0.75, 1.0)
    )


def is_arc_prev(curve: Sequence[float], circle: Circle) -> bool:
    """Checks whether the previous curve lies on the same circle."""
    return is_arc(
        curve,
        Circle(
            center=[circle.center[0] + curve[4], circle.center[1] + curve[5]],
            radius=circle.radius,
        ),
    )


def find_arc_angle(curve: Sequence[float], rel_circle: Circle) -> float:
    """Computes the arc angle subtended by the curve on rel_circle."""
    x1 = -rel_circle.center[0]
    y1 = -rel_circle.center[1]
    x2 = curve[4] - rel_circle.center[0]
    y2 = curve[5] - rel_circle.center[1]
    denom = math.sqrt((x1 * x1 + y1 * y1) * (x2 * x2 + y2 * y2))
    if denom == 0:
        return math.nan
    cos_val = (x1 * x2 + y1 * y2) / denom
    if cos_val < -1.0 or cos_val > 1.0:
        return math.nan
    return math.acos(cos_val)


def data_to_path(path_data: Sequence[PathItem]) -> str:
    """Serializes PathItem objects into an SVG path string."""
    parts = []
    for item in path_data:
        str_data = (
            cleanup_out_data(strong_round(list(item.args))) if item.args else ''
        )
        parts.append(item.command + str_data)
    return ''.join(parts)


def _convert_to_relative(path_data: list[PathItem]) -> None:
    """Converts absolute path items into relative path items in-place."""
    start = [0.0, 0.0]
    cursor = [0.0, 0.0]
    prev_coords = [0.0, 0.0]
    for i, path_item in enumerate(path_data):
        command = path_item.command
        args = path_item.args
        if command == 'M':
            if i != 0:
                command = 'm'
            args[0] -= cursor[0]
            args[1] -= cursor[1]
            cursor[0] += args[0]
            cursor[1] += args[1]
            start[0] = cursor[0]
            start[1] = cursor[1]
        elif command == 'L':
            command = 'l'
            args[0] -= cursor[0]
            args[1] -= cursor[1]
            cursor[0] += args[0]
            cursor[1] += args[1]
        elif command == 'C':
            command = 'c'
            args[0] -= cursor[0]
            args[1] -= cursor[1]
            args[2] -= cursor[0]
            args[3] -= cursor[1]
            args[4] -= cursor[0]
            args[5] -= cursor[1]
            cursor[0] += args[4]
            cursor[1] += args[5]
        elif command in ('Z', 'z'):
            cursor[0] = start[0]
            cursor[1] = start[1]
        path_item.command = command
        path_item.args = args
        path_item.base = prev_coords
        path_item.coords = [cursor[0], cursor[1]]
        prev_coords = path_item.coords


def _convert_to_mixed(path_data: list[PathItem]) -> None:
    """Switches relative commands to absolute when shorter."""
    if not path_data:
        return
    prev = path_data[0]
    for item in path_data[1:]:
        if item.command in ('Z', 'z'):
            prev = item
            continue
        command = item.command
        data = item.args
        adata = list(data)
        rdata = list(data)
        if command in ('m', 'l', 's', 'c'):
            for k in range(len(adata) - 1, -1, -1):
                adata[k] += item.base[k % 2]
        elif command == 'h':
            adata[0] += item.base[0]
        elif command == 'v':
            adata[0] += item.base[1]
        elif command == 'a':
            adata[5] += item.base[0]
            adata[6] += item.base[1]

        strong_round(adata)
        strong_round(rdata)
        absolute_data_str = cleanup_out_data(adata)
        relative_data_str = cleanup_out_data(rdata)
        if len(absolute_data_str) < len(relative_data_str) and not (
            command == prev.command
            and prev.command.islower()
            and len(absolute_data_str) == len(relative_data_str) - 1
            and (
                data[0] < 0
                or (
                    math.floor(data[0]) == 0
                    and not float(data[0]).is_integer()
                    and not float(prev.args[-1]).is_integer()
                )
            )
        ):
            item.command = command.upper()
            item.args = adata
        prev = item


def optimize_path_data(path_data: list[PathItem]) -> list[PathItem]:
    """Replicates SVGO's convertPathData optimization pass."""
    _convert_to_relative(path_data)

    rel_subpoint = [0.0, 0.0]
    path_base = [0.0, 0.0]
    prev = PathItem(command='', args=[])
    filtered_path: list[PathItem] = []

    index = 0
    while index < len(path_data):
        item = path_data[index]
        command = item.command
        data = item.args
        next_item = path_data[index + 1] if index + 1 < len(path_data) else None

        if command not in ('Z', 'z'):
            sdata = data
            circle: Optional[Circle] = None
            if command == 's':
                sdata = [
                    prev.args[-2] - prev.args[-4],
                    prev.args[-1] - prev.args[-3],
                ] + data

            if command in ('c', 's') and is_convex(sdata):
                circle = find_circle(sdata)

            if circle is not None:
                r = strong_round([circle.radius])[0]
                angle = find_arc_angle(sdata, circle)
                sweep = (
                    1.0
                    if sdata[5] * sdata[0] - sdata[4] * sdata[1] > 0
                    else 0.0
                )
                arc = PathItem(
                    command='a',
                    args=[r, r, 0.0, 0.0, sweep, sdata[4], sdata[5]],
                    coords=list(item.coords),
                    base=item.base,
                )
                output: list[PathItem] = [arc]
                rel_center = [
                    circle.center[0] - sdata[4],
                    circle.center[1] - sdata[5],
                ]
                rel_circle = Circle(center=rel_center, radius=circle.radius)
                arc_curves: list[PathItem] = [item]
                has_prev = 0
                suffix = ''

                prev_is_arc = (
                    prev.command == 'c'
                    and is_convex(prev.args)
                    and is_arc_prev(prev.args, circle)
                ) or (
                    prev.command == 'a'
                    and prev.sdata is not None
                    and is_arc_prev(prev.sdata, circle)
                )
                if prev_is_arc:
                    arc_curves.insert(0, prev)
                    arc.base = prev.base
                    arc.args[5] = arc.coords[0] - arc.base[0]
                    arc.args[6] = arc.coords[1] - arc.base[1]
                    prev_data = (
                        prev.sdata
                        if prev.command == 'a' and prev.sdata is not None
                        else prev.args
                    )
                    prev_angle = find_arc_angle(
                        prev_data,
                        Circle(
                            center=[
                                prev_data[4] + circle.center[0],
                                prev_data[5] + circle.center[1],
                            ],
                            radius=circle.radius,
                        ),
                    )
                    angle += prev_angle
                    if angle > math.pi:
                        arc.args[3] = 1.0
                    has_prev = 1

                j = index + 1
                while j < len(path_data):
                    next_item = path_data[j]
                    if next_item.command not in ('c', 's'):
                        break
                    next_data = next_item.args
                    if next_item.command == 's':
                        next_longhand = make_longhand(
                            PathItem(command='s', args=list(next_item.args)),
                            path_data[j - 1].args,
                        )
                        next_data = next_longhand.args
                        next_longhand.args = next_data[:2]
                        suffix = data_to_path([next_longhand])
                    if is_convex(next_data) and is_arc(next_data, rel_circle):
                        angle += find_arc_angle(next_data, rel_circle)
                        if angle - 2 * math.pi > 1e-3:
                            break
                        if angle > math.pi:
                            arc.args[3] = 1.0
                        arc_curves.append(next_item)
                        if 2 * math.pi - angle > 1e-3:
                            arc.coords = next_item.coords
                            arc.args[5] = arc.coords[0] - arc.base[0]
                            arc.args[6] = arc.coords[1] - arc.base[1]
                        else:
                            arc.args[5] = 2 * (
                                rel_circle.center[0] - next_data[4]
                            )
                            arc.args[6] = 2 * (
                                rel_circle.center[1] - next_data[5]
                            )
                            arc.coords = [
                                arc.base[0] + arc.args[5],
                                arc.base[1] + arc.args[6],
                            ]
                            arc = PathItem(
                                command='a',
                                args=[
                                    r,
                                    r,
                                    0.0,
                                    0.0,
                                    sweep,
                                    next_item.coords[0] - arc.coords[0],
                                    next_item.coords[1] - arc.coords[1],
                                ],
                                coords=next_item.coords,
                                base=arc.coords,
                            )
                            output.append(arc)
                            j += 1
                            break
                        rel_center[0] -= next_data[4]
                        rel_center[1] -= next_data[5]
                    else:
                        break
                    j += 1
                else:
                    next_item = None

                if len(data_to_path(output) + suffix) < len(
                    data_to_path(arc_curves)
                ):
                    if j < len(path_data) and path_data[j].command == 's':
                        make_longhand(path_data[j], path_data[j - 1].args)
                    if has_prev:
                        prev_arc = output.pop(0)
                        strong_round(prev_arc.args)
                        rel_subpoint[0] += prev_arc.args[5] - prev.args[-2]
                        rel_subpoint[1] += prev_arc.args[6] - prev.args[-1]
                        prev.command = 'a'
                        prev.args = prev_arc.args
                        item.base = prev.coords = prev_arc.coords
                    arc_opt = output.pop(0) if output else None
                    del_count = len(arc_curves) - 1 - has_prev
                    if len(arc_curves) == 1:
                        item.sdata = list(sdata)
                    elif del_count > 0:
                        path_data[index + 1 : index + 1 + del_count] = output
                    if arc_opt is None:
                        index += 1
                        continue
                    command = 'a'
                    data = arc_opt.args
                    item.coords = arc_opt.coords

            if command in ('m', 'l', 's', 'c'):
                for k in range(len(data) - 1, -1, -1):
                    data[k] += item.base[k % 2] - rel_subpoint[k % 2]
            elif command == 'h':
                data[0] += item.base[0] - rel_subpoint[0]
            elif command == 'v':
                data[0] += item.base[1] - rel_subpoint[1]
            elif command == 'a':
                data[5] += item.base[0] - rel_subpoint[0]
                data[6] += item.base[1] - rel_subpoint[1]
            strong_round(data)

            if command == 'h':
                rel_subpoint[0] += data[0]
            elif command == 'v':
                rel_subpoint[1] += data[0]
            else:
                rel_subpoint[0] += data[-2]
                rel_subpoint[1] += data[-1]
            strong_round(rel_subpoint)

            if command in ('M', 'm'):
                path_base[0] = rel_subpoint[0]
                path_base[1] = rel_subpoint[1]

            sagitta = calculate_sagitta(data) if command == 'a' else None
            if sagitta is not None:
                for prec_new in range(PRECISION, -1, -1):
                    radius = to_fixed(data[0], prec_new)
                    sagitta_new = calculate_sagitta([radius, radius] + data[2:])
                    if (
                        sagitta_new is not None
                        and abs(sagitta - sagitta_new) < ERROR
                    ):
                        data[0] = radius
                        data[1] = radius
                    else:
                        break

            if (command == 'c' and is_curve_straight_line(data)) or (
                command == 's' and is_curve_straight_line(sdata)
            ):
                if next_item is not None and next_item.command == 's':
                    make_longhand(next_item, data)
                command = 'l'
                data = data[-2:]
            elif command == 'a' and (
                data[0] == 0
                or data[1] == 0
                or (sagitta is not None and sagitta < ERROR)
            ):
                command = 'l'
                data = data[-2:]

            if command == 'l':
                if data[1] == 0:
                    command = 'h'
                    data.pop()
                elif data[0] == 0:
                    command = 'v'
                    data.pop(0)

            if (
                command in ('m', 'h', 'v')
                and prev.command
                and command == prev.command.lower()
                and (
                    command not in ('h', 'v')
                    or ((prev.args[0] >= 0) == (data[0] >= 0))
                )
            ):
                prev.args[0] += data[0]
                if command not in ('h', 'v'):
                    prev.args[1] += data[1]
                prev.coords = item.coords
                path_data[index] = prev
                index += 1
                continue

            if prev.command and command == 'c':
                if (
                    prev.command == 'c'
                    and abs(data[0] - -(prev.args[2] - prev.args[4])) < ERROR
                    and abs(data[1] - -(prev.args[3] - prev.args[5])) < ERROR
                ):
                    command = 's'
                    data = data[2:]
                elif (
                    prev.command == 's'
                    and abs(data[0] - -(prev.args[0] - prev.args[2])) < ERROR
                    and abs(data[1] - -(prev.args[1] - prev.args[3])) < ERROR
                ):
                    command = 's'
                    data = data[2:]
                elif (
                    prev.command not in ('c', 's')
                    and abs(data[0]) < ERROR
                    and abs(data[1]) < ERROR
                ):
                    command = 's'
                    data = data[2:]

            if command in ('l', 'h', 'v', 'c', 's') and all(
                x == 0 for x in data
            ):
                path_data[index] = prev
                index += 1
                continue
            if command == 'a' and data[5] == 0 and data[6] == 0:
                path_data[index] = prev
                index += 1
                continue

            if (
                command in ('l', 'h', 'v')
                and abs(path_base[0] - item.coords[0]) < ERROR
                and abs(path_base[1] - item.coords[1]) < ERROR
            ):
                command = 'z'
                data = []

            item.command = command
            item.args = data
        else:
            rel_subpoint[0] = path_base[0]
            rel_subpoint[1] = path_base[1]
            if prev.command in ('Z', 'z'):
                index += 1
                continue

        if (
            command in ('Z', 'z')
            and prev.command in ('M', 'm')
            and abs(item.base[0] - item.coords[0]) < ERROR / 10
            and abs(item.base[1] - item.coords[1]) < ERROR / 10
        ):
            index += 1
            continue

        prev = item
        filtered_path.append(item)
        index += 1

    path_data = filtered_path
    _convert_to_mixed(path_data)

    # Match SVGO js2path: M followed by l at start of path becomes m.
    if (
        len(path_data) >= 2
        and path_data[0].command == 'M'
        and path_data[1].command == 'l'
    ):
        path_data[0].command = 'm'

    return path_data
