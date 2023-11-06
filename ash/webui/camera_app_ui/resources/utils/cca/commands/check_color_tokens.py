# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import re

from cca import cli
from typing import List


def glob_with_ignore(pattern: str, ignore_list: List[str]):
    for filename in glob.glob(pattern, recursive=True):
        if filename in ignore_list:
            continue
        if filename.startswith('dist/'):
            # The file is generated from cca.py bundle
            # TODO(pihsun): respect .gitignore here?
            continue
        yield filename


# Ref: https://developer.mozilla.org/en-US/docs/Web/CSS/named-color
_CSS_NAMED_COLORS = """
aliceblue antiquewhite aqua aquamarine azure beige bisque black blanchedalmond
blue blueviolet brown burlywood cadetblue chartreuse chocolate coral
cornflowerblue cornsilk crimson cyan darkblue darkcyan darkgoldenrod darkgray
darkgreen darkgrey darkkhaki darkmagenta darkolivegreen darkorange darkorchid
darkred darksalmon darkseagreen darkslateblue darkslategray darkslategrey
darkturquoise darkviolet deeppink deepskyblue dimgray dimgrey dodgerblue
firebrick floralwhite forestgreen fuchsia gainsboro ghostwhite gold goldenrod
gray green greenyellow grey honeydew hotpink indianred indigo ivory khaki
lavender lavenderblush lawngreen lemonchiffon lightblue lightcoral lightcyan
lightgoldenrodyellow lightgray lightgreen lightgrey lightpink lightsalmon
lightseagreen lightskyblue lightslategray lightslategrey lightsteelblue
lightyellow lime limegreen linen magenta maroon mediumaquamarine mediumblue
mediumorchid mediumpurple mediumseagreen mediumslateblue mediumspringgreen
mediumturquoise mediumvioletred midnightblue mintcream mistyrose moccasin
navajowhite navy oldlace olive olivedrab orange orangered orchid palegoldenrod
palegreen paleturquoise palevioletred papayawhip peachpuff peru pink plum
powderblue purple rebeccapurple red rosybrown royalblue saddlebrown salmon
sandybrown seagreen seashell sienna silver skyblue slateblue slategray
slategrey snow springgreen steelblue tan teal thistle tomato turquoise violet
wheat white whitesmoke yellow yellowgreen
""".split()
_CSS_NAMED_COLORS_REGEX = "|".join(_CSS_NAMED_COLORS)

# named color or #rgb / #rrggbb / #rrggbbaa
_CSS_COLOR_REGEX = f"{_CSS_NAMED_COLORS_REGEX}|#[0-9a-fA-F]{{3,8}}"

# utils/cca/static/colors.css is used as a stub for local dev.
_CSS_ALLOWLIST = ["utils/cca/static/colors.css"]


def _check_color_tokens_css() -> int:
    returncode = 0

    def print_error(filename, lineno, msg):
        nonlocal returncode
        print(f"{filename}:{lineno} - {msg}")
        returncode = 1

    for filename in glob_with_ignore("**/*.css", _CSS_ALLOWLIST):
        with open(filename) as f:
            css_lines = f.read().splitlines()

        ignore_next_line = False
        for lineno, line in enumerate(css_lines, 1):
            if "color-token-disable-next-line" in line:
                ignore_next_line = True
                continue
            if ignore_next_line:
                ignore_next_line = False
                continue

            line = line.strip()
            # Ignore comments
            if line.startswith("/*"):
                continue

            # Check all rgb() / rgba() uses are for box-shadow.
            # This is a heuristic since this doesn't consider multi-line rule
            # for now.
            # TODO(pihsun): Use CSS variables for different kind of box-shadow,
            # and remove this special casing of box-shadow.
            if re.search(r"rgba?\(",
                         line) and not line.startswith("box-shadow: "):
                print_error(filename, lineno, "hardcoded rgba() value found.")

            # Check for color names and hexadecimal notations.
            match = re.search(
                # start of line or space
                "(?:^|[ ])"
                # ... followed by color
                f"({_CSS_COLOR_REGEX})"
                # .. followed by end of line or space or ;
                "(?:$|[ ;])",
                line,
            )
            if match is not None:
                print_error(filename, lineno,
                            f'hardcoded color "{match[1]}" found.')

    return returncode


_SVG_ALLOWLIST = [
    # This image is only used as -webkit-mask, which needs to have
    # solid fill color but the fill color itself is not used.
    "images/barcode_scan_box_border_mask.svg",
]


def _check_color_tokens_svg() -> int:
    returncode = 0

    def print_error(filename, lineno, msg):
        nonlocal returncode
        print(f"{filename}:{lineno} - {msg}")
        returncode = 1

    for filename in glob_with_ignore("**/*.svg", _SVG_ALLOWLIST):
        with open(filename) as f:
            svg_lines = f.read().splitlines()

        for lineno, line in enumerate(svg_lines, 1):
            line = line.strip()
            # Check for color names and hexadecimal notations.
            match = re.search(
                # start of line or space (for inline CSS) or {fill,stroke}="
                '(?:^|[ ]|fill="|stroke=")'
                # ... followed by color
                f"({_CSS_COLOR_REGEX})"
                # .. followed by end of line or space or ; or "
                '(?:$|[ ;"])',
                line,
            )
            if match is not None:
                print_error(
                    filename,
                    lineno,
                    f'hardcoded color "{match[1]}" found. '
                    "Please omit the fill/stroke value and specify it in CSS, "
                    "or use var(--secondary-color) if two colors are needed.",
                )

    return returncode


@cli.command(
    "check-color-tokens",
    help="check color token usage in CSS and SVG files",
    description="""Ensure all CSS files and SVG files.""",
)
def cmd() -> int:
    """Checks all colors used in CSS and SVG files are using color tokens."""
    returncode = 0
    returncode |= _check_color_tokens_css()
    returncode |= _check_color_tokens_svg()
    return returncode
