# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Serializes optimized SVG PathItem sequences into Chromium .icon blocks."""

from collections.abc import Sequence
import math

from svg_resizer import PathItem, js_num_str  # pytype: disable=import-error

SVG_TO_ICON_COMMAND = {
    'M': 'MOVE_TO',
    'm': 'R_MOVE_TO',
    'L': 'LINE_TO',
    'l': 'R_LINE_TO',
    'H': 'H_LINE_TO',
    'h': 'R_H_LINE_TO',
    'V': 'V_LINE_TO',
    'v': 'R_V_LINE_TO',
    'A': 'ARC_TO',
    'a': 'R_ARC_TO',
    'C': 'CUBIC_TO',
    'S': 'CUBIC_TO_SHORTHAND',
    'c': 'R_CUBIC_TO',
    's': 'R_CUBIC_TO',
    'Z': 'CLOSE',
    'z': 'CLOSE',
}


def to_command(letter: str) -> str:
    """Maps an SVG path command letter to its Chromium .icon directive."""
    return SVG_TO_ICON_COMMAND.get(letter, '~UNKNOWN~')


def round_to_hundredths(x: float) -> float:
    """Rounds a coordinate to 2 decimal places (half towards +Infinity)."""
    return math.floor(x * 100 + 0.5) / 100


def path_data_to_icon_block(
    path_data: Sequence[PathItem], target_size: int, nonzero_fill: bool = True
) -> str:
    """Converts optimized PathItem sequence into a single-size .icon block.

    When nonzero_fill is False, FILL_RULE_NONZERO is omitted so that
    PaintVectorIcon uses its default even-odd fill type.
    """
    output = f'CANVAS_DIMENSIONS, {target_size},\n'
    if nonzero_fill:
        output += 'FILL_RULE_NONZERO,\n'
    commands: list[PathItem] = []

    for item in path_data:
        svg_directive = item.command
        raw_args = item.args
        current_args: list[float] = []
        if svg_directive == 's':
            last_command = commands[-1] if commands else None
            if last_command is not None and 'CUBIC_TO' in to_command(
                last_command.command
            ):
                lgth = len(last_command.args)
                current_args.append(
                    round_to_hundredths(
                        last_command.args[lgth - 2]
                        - last_command.args[lgth - 4]
                    )
                )
                current_args.append(
                    round_to_hundredths(
                        last_command.args[lgth - 1]
                        - last_command.args[lgth - 3]
                    )
                )
            else:
                current_args.extend([0.0, 0.0])
        for a in raw_args:
            current_args.append(round_to_hundredths(a))
        commands.append(PathItem(command=svg_directive, args=current_args))

    while (
        commands
        and commands[-1].command in ('M', 'm')
        and commands[-1].args == [0.0, 0.0]
    ):
        commands.pop()

    if not commands or commands[-1].command.lower() != 'z':
        commands.append(PathItem(command='z', args=[]))

    for command in commands:
        line = f'{to_command(command.command)}, '
        for point in command.args:
            line += js_num_str(point)
            if not float(point).is_integer():
                line += 'f'
            line += ', '
        output += line.strip() + '\n'

    content = output.strip()
    if content.endswith(','):
        content = content[:-1]
    return content
