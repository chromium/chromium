# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses single-<path> SVGs, scales coordinates, and elevates curves."""

from collections.abc import Sequence
import dataclasses
import math
import re
from typing import Optional
import xml.etree.ElementTree as ET

MAX_CUSTOM_ICON_SIZE = 64


@dataclasses.dataclass
class PathItem:
    """Represents a single SVG path command and its coordinates."""

    command: str
    args: list[float]
    base: list[float] = dataclasses.field(default_factory=lambda: [0.0, 0.0])
    coords: list[float] = dataclasses.field(default_factory=lambda: [0.0, 0.0])
    sdata: Optional[list[float]] = None


def js_num_str(val: float) -> str:
    """Formats a float matching JavaScript Number.prototype.toString()."""
    if val == 0:
        return '0'
    if math.isfinite(val) and float(val).is_integer():
        return str(int(val))
    return str(val)


def _local_name(tag: str) -> str:
    """Strips the XML namespace from an element tag."""
    return tag.rsplit('}', 1)[-1]


def _get_presentation_attr(elem: ET.Element, name: str) -> Optional[str]:
    """Returns a presentation attribute, preferring inline style values."""
    for declaration in (elem.get('style') or '').split(';'):
        key, sep, value = declaration.partition(':')
        if sep and key.strip() == name:
            return value.strip()
    return elem.get(name)


def _parse_svg(svg_content: str) -> tuple[ET.Element, ET.Element]:
    """Parses and validates an <svg> containing exactly one <path>."""
    try:
        root = ET.fromstring(svg_content)
    except ET.ParseError as exc:
        raise ValueError(f'Failed to parse SVG XML: {exc}') from exc
    if _local_name(root.tag) != 'svg':
        raise ValueError('The root element must be <svg>.')

    elements = list(root.iter())
    unsupported_tags = [
        _local_name(e.tag)
        for e in elements
        if _local_name(e.tag) not in ('svg', 'path')
    ]
    if unsupported_tags:
        unique_unsupported = ', '.join(
            f'<{t}>' for t in dict.fromkeys(unsupported_tags)
        )
        raise ValueError(
            f'Unsupported SVG element(s): {unique_unsupported}. '
            'Only <svg> containing a single <path> is supported.'
        )
    paths = [e for e in elements if _local_name(e.tag) == 'path']
    if len(paths) != 1:
        raise ValueError(
            f'SVG must contain exactly one <path> element (found {len(paths)}).'
        )

    for elem in (root, paths[0]):
        if _get_presentation_attr(elem, 'transform') is not None:
            raise ValueError(
                'SVG transforms are not supported. Flatten the transform into '
                'the path data before converting.'
            )
        if _get_presentation_attr(elem, 'stroke') not in (None, 'none'):
            raise ValueError(
                'Stroked SVG paths are not supported. Convert strokes to '
                'filled outlines before converting.'
            )
    return root, paths[0]


def _parse_length(value: str, name: str) -> float:
    """Parses a positive unitless or px SVG length."""
    match = re.fullmatch(r'\s*(\d*\.?\d+)(?:px)?\s*', value)
    if not match or float(match.group(1)) <= 0:
        raise ValueError(
            f'Unsupported SVG {name} "{value}". Expected a positive unitless '
            'or px length.'
        )
    return float(match.group(1))


def _parse_svg_bounds(root: ET.Element) -> tuple[float, float, float, float]:
    """Extracts square (min_x, min_y, width, height) bounds from <svg>."""
    raw_width = root.get('width')
    raw_height = root.get('height')
    width = _parse_length(raw_width, 'width') if raw_width else None
    height = _parse_length(raw_height, 'height') if raw_height else None
    if width is not None and height is not None and width != height:
        raise ValueError(
            f'SVG width and height must be equal (got {raw_width} x '
            f'{raw_height}). Chromium .icon canvases are square.'
        )

    raw_vb = root.get('viewBox')
    if raw_vb is not None:
        try:
            vb_parts = [
                float(x) for x in re.split(r'[,\s]+', raw_vb.strip()) if x
            ]
        except ValueError:
            vb_parts = []
        if len(vb_parts) != 4:
            raise ValueError(f'Malformed SVG viewBox "{raw_vb}".')
        min_x, min_y, vb_width, vb_height = vb_parts
    elif width is not None and height is not None:
        min_x, min_y, vb_width, vb_height = 0.0, 0.0, width, height
    else:
        raise ValueError('SVG must specify a viewBox or both width and height.')

    if vb_width <= 0 or vb_height <= 0:
        raise ValueError(f'SVG viewBox dimensions must be positive: {raw_vb}')
    if vb_width != vb_height:
        raise ValueError(
            f'SVG viewBox must be square (got {vb_width:g} x {vb_height:g}). '
            'Chromium .icon canvases are square.'
        )
    return min_x, min_y, vb_width, vb_height


def has_evenodd_fill_rule(svg_content: str) -> bool:
    """Returns True if the SVG <path> uses fill-rule="evenodd"."""
    root, path = _parse_svg(svg_content)
    fill_rule = _get_presentation_attr(
        path, 'fill-rule'
    ) or _get_presentation_attr(root, 'fill-rule')
    return fill_rule == 'evenodd'


def infer_svg_size(svg_content: str) -> int:
    """Infers the default target canvas size from an SVG's attributes."""
    root, _ = _parse_svg(svg_content)
    _, _, vb_width, _ = _parse_svg_bounds(root)
    raw_width = root.get('width')
    size = _parse_length(raw_width, 'width') if raw_width else vb_width
    inferred = int(round(size))
    if 0 < inferred <= MAX_CUSTOM_ICON_SIZE:
        return inferred
    raise ValueError(
        f'Inferred SVG size ({inferred}) is not a valid Chromium DIP icon '
        f'size (1-{MAX_CUSTOM_ICON_SIZE}). Double-check the intended DIP size '
        'and pass --sizes <size> explicitly (e.g. --sizes 20).'
    )


def resize_and_elevate_to_path_data(
    svg_content: str, target_size: int
) -> list[PathItem]:
    """Resizes SVG path to target_size and elevates quadratic curves to cubic.

    Replicates Cairo's 24.8 fixed-point coordinate rounding and degree
    elevation from rsvg-convert. Requires a single-<path> SVG.
    """
    if target_size <= 0:
        raise ValueError(
            f'Target size must be a positive integer, got {target_size}.'
        )
    root, path = _parse_svg(svg_content)
    svg_path_d = path.get('d')
    if not svg_path_d:
        raise ValueError('The SVG <path> has no "d" attribute.')

    min_x, min_y, vb_width, vb_height = _parse_svg_bounds(root)

    scale_x = target_size / vb_width
    scale_y = target_size / vb_height

    def cairo_round(v: float) -> float:
        return round(math.floor(v * 256 + 0.5) / 256, 6)

    def tx(x: float) -> float:
        return cairo_round((x - min_x) * scale_x)

    def ty(y: float) -> float:
        return cairo_round((y - min_y) * scale_y)

    tokens = re.findall(
        r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?|[a-zA-Z]', svg_path_d
    )
    i = 0
    cmd = ''
    cx = 0.0
    cy = 0.0
    sx = 0.0
    sy = 0.0
    qx: Optional[float] = None
    qy: Optional[float] = None
    cx_ctrl: Optional[float] = None
    cy_ctrl: Optional[float] = None
    path_data: list[PathItem] = []

    def next_float() -> float:
        nonlocal i
        if i >= len(tokens) or tokens[i].isalpha():
            raise ValueError(
                f'Truncated parameter list for SVG path command "{cmd}".'
            )
        val = float(tokens[i])
        i += 1
        return val

    while i < len(tokens):
        if re.match(r'^[a-zA-Z]$', tokens[i]):
            cmd = tokens[i]
            i += 1
            if cmd in ('Z', 'z'):
                path_data.append(PathItem(command='Z', args=[]))
                cx = sx
                cy = sy
                qx = qy = None
                cx_ctrl = cy_ctrl = None
            continue

        if cmd in ('M', 'm'):
            x = next_float()
            y = next_float()
            if cmd == 'm':
                x += cx
                y += cy
            cx = sx = x
            cy = sy = y
            qx = qy = None
            cx_ctrl = cy_ctrl = None
            path_data.append(PathItem(command='M', args=[tx(cx), ty(cy)]))
            cmd = 'L' if cmd == 'M' else 'l'
        elif cmd in ('L', 'l'):
            x = next_float()
            y = next_float()
            if cmd == 'l':
                x += cx
                y += cy
            cx = x
            cy = y
            qx = qy = None
            cx_ctrl = cy_ctrl = None
            path_data.append(PathItem(command='L', args=[tx(cx), ty(cy)]))
        elif cmd in ('H', 'h'):
            x = next_float()
            if cmd == 'h':
                x += cx
            cx = x
            qx = qy = None
            cx_ctrl = cy_ctrl = None
            path_data.append(PathItem(command='L', args=[tx(cx), ty(cy)]))
        elif cmd in ('V', 'v'):
            y = next_float()
            if cmd == 'v':
                y += cy
            cy = y
            qx = qy = None
            cx_ctrl = cy_ctrl = None
            path_data.append(PathItem(command='L', args=[tx(cx), ty(cy)]))
        elif cmd in ('Q', 'q', 'T', 't'):
            if cmd in ('Q', 'q'):
                x1 = next_float()
                y1 = next_float()
                x = next_float()
                y = next_float()
                if cmd == 'q':
                    x1 += cx
                    y1 += cy
                    x += cx
                    y += cy
            else:
                x = next_float()
                y = next_float()
                if cmd == 't':
                    x += cx
                    y += cy
                if qx is not None and qy is not None:
                    x1 = 2 * cx - qx
                    y1 = 2 * cy - qy
                else:
                    x1 = cx
                    y1 = cy
            c1x = cx + (2.0 / 3.0) * (x1 - cx)
            c1y = cy + (2.0 / 3.0) * (y1 - cy)
            c2x = x + (2.0 / 3.0) * (x1 - x)
            c2y = y + (2.0 / 3.0) * (y1 - y)
            qx = x1
            qy = y1
            cx_ctrl = cy_ctrl = None
            cx = x
            cy = y
            path_data.append(
                PathItem(
                    command='C',
                    args=[
                        tx(c1x),
                        ty(c1y),
                        tx(c2x),
                        ty(c2y),
                        tx(cx),
                        ty(cy),
                    ],
                )
            )
        elif cmd in ('C', 'c', 'S', 's'):
            if cmd in ('C', 'c'):
                c1x = next_float()
                c1y = next_float()
                c2x = next_float()
                c2y = next_float()
                x = next_float()
                y = next_float()
                if cmd == 'c':
                    c1x += cx
                    c1y += cy
                    c2x += cx
                    c2y += cy
                    x += cx
                    y += cy
            else:
                c2x = next_float()
                c2y = next_float()
                x = next_float()
                y = next_float()
                if cmd == 's':
                    c2x += cx
                    c2y += cy
                    x += cx
                    y += cy
                if cx_ctrl is not None and cy_ctrl is not None:
                    c1x = 2 * cx - cx_ctrl
                    c1y = 2 * cy - cy_ctrl
                else:
                    c1x = cx
                    c1y = cy
            cx_ctrl = c2x
            cy_ctrl = c2y
            qx = qy = None
            cx = x
            cy = y
            path_data.append(
                PathItem(
                    command='C',
                    args=[
                        tx(c1x),
                        ty(c1y),
                        tx(c2x),
                        ty(c2y),
                        tx(cx),
                        ty(cy),
                    ],
                )
            )
        else:
            raise ValueError(
                f'Unsupported SVG path command: {cmd}. Only M, L, H, V, C, S, '
                'Q, T, and Z (and relative forms) are supported. Arc commands '
                '(A/a) must be converted to cubic curves first.'
            )

    return _simplify_cairo_path(path_data)


def _cairo_slope_compare(dx1: int, dy1: int, dx2: int, dy2: int) -> bool:
    """Returns True if 24.8 fixed-point vectors are collinear and same-dir."""
    if (dx1 == 0 and dy1 == 0) or (dx2 == 0 and dy2 == 0):
        return True
    if (dx1 > 0) != (dx2 > 0) or (dy1 > 0) != (dy2 > 0):
        return False
    return dx1 * dy2 == dy1 * dx2


def _simplify_cairo_path(path_data: Sequence[PathItem]) -> list[PathItem]:
    """Replicates Cairo's fixed-point line_to and close_path simplification."""
    out: list[PathItem] = []
    cur_pt = (0, 0)
    last_move_pt = (0, 0)
    prev_to_last_pt = (0, 0)

    def to_fixed_pt(x: float, y: float) -> tuple[int, int]:
        return (int(round(x * 256)), int(round(y * 256)))

    def append_line_to(pt: tuple[int, int], float_args: list[float]) -> None:
        nonlocal cur_pt, prev_to_last_pt
        if pt == cur_pt:
            return
        if out and out[-1].command == 'L':
            dx1 = cur_pt[0] - prev_to_last_pt[0]
            dy1 = cur_pt[1] - prev_to_last_pt[1]
            dx2 = pt[0] - cur_pt[0]
            dy2 = pt[1] - cur_pt[1]
            if _cairo_slope_compare(dx1, dy1, dx2, dy2):
                out[-1].args = float_args
                cur_pt = pt
                return
        prev_to_last_pt = cur_pt
        cur_pt = pt
        out.append(PathItem(command='L', args=float_args))

    for item in path_data:
        cmd = item.command
        if cmd == 'M':
            pt = to_fixed_pt(item.args[0], item.args[1])
            if out and out[-1].command == 'M':
                out[-1] = PathItem(command='M', args=list(item.args))
            else:
                out.append(PathItem(command='M', args=list(item.args)))
            cur_pt = pt
            last_move_pt = pt
        elif cmd == 'L':
            pt = to_fixed_pt(item.args[0], item.args[1])
            append_line_to(pt, list(item.args))
        elif cmd == 'C':
            pt = to_fixed_pt(item.args[4], item.args[5])
            prev_to_last_pt = cur_pt
            cur_pt = pt
            out.append(PathItem(command='C', args=list(item.args)))
        elif cmd in ('Z', 'z'):
            append_line_to(
                last_move_pt,
                [
                    round(last_move_pt[0] / 256, 6),
                    round(last_move_pt[1] / 256, 6),
                ],
            )
            if out and out[-1].command == 'L' and cur_pt == last_move_pt:
                out.pop()
            out.append(PathItem(command='Z', args=[]))
            cur_pt = last_move_pt

    return out
