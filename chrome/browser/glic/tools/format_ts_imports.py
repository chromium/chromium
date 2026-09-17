#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Rewrites long TypeScript import statements to multi-line with trailing comments.

When TypeScript imports are long (>120 chars) and have more than 2 symbols,
clang-format might wrap them awkwardly. Adding a trailing ',//' after each
imported symbol forces clang-format to format each imported symbol onto its
own line with aligned comments.

Handles existing comments (block comments /* ... */ and line comments // ...)
mixed into the import statements cleanly. Clang-format is always run on the result.

Usage:
  python3 chrome/browser/glic/tools/format_ts_imports.py <file.ts> [--in-place] [--check-only] [--threshold 120] [--min-symbols 2]


Note, you can configure jj to automatically run this script with jj fix, with
this addition in jj config.

[fix.tools.0-format-ts-imports]
command = ["chrome/browser/glic/tools/format_ts_imports.py", "-"]
patterns = [
    "glob:'chrome/browser/resources/glic/**/*.ts'",
]

"""

import argparse
import re
import subprocess
import sys
from pathlib import Path


# Matches named TypeScript / JavaScript imports across single or multiple lines:
#   import { A, B } from './path.js';
#   import type { A, B } from './path.js';
#   import Default, { A, B } from './path.js';
#   import type Default, { A, B } from './path.js';
IMPORT_RE = re.compile(
    r"^([ \t]*import(?:\s+(?:type|/\*.*?\*/))*[ \t]*(?:[\w$]+[ \t]*,[ \t]*)?)\{([\s\S]*?)\}([ \t]*(?:/\*.*?\*/[ \t]*)?from[ \t]+['\"][^'\"]+['\"][ \t]*;?[ \t]*(?://[^\n]*)?)",
    re.MULTILINE,
)


def extract_symbols(inside: str) -> list[str]:
    """Extracts imported symbols from inside `{ ... }`, stripping any comments."""
    # 1. Strip block comments (/* ... */)
    no_blocks = re.sub(r"/\*.*?\*/", "", inside, flags=re.DOTALL)
    # 2. Strip line comments (// ...)
    no_lines = re.sub(r"//[^\n]*", "", no_blocks)
    # 3. Split by commas and strip whitespace
    return [item.strip() for item in no_lines.split(",") if item.strip()]


def rewrite_imports(
    content: str,
    threshold: int = 120,
    min_symbols: int = 2,
) -> str:
    """Finds named imports exceeding `threshold` chars with > `min_symbols` and formats them multi-line with ',//'.

    Also removes internal trailing comments and collapses to single-line if the import is short enough.
    """

    def replace_import(match: re.Match) -> str:
        full_match = match.group(0)

        prefix = match.group(1).rstrip()
        inside = match.group(2)
        suffix = match.group(3).lstrip()

        symbols = extract_symbols(inside)
        if not symbols:
            return full_match

        # Measure simulated single-line length without comments
        clean_suffix = re.sub(r"//.*$", "", suffix).strip()
        simulated_single = f"{prefix} {{{', '.join(symbols)}}} {clean_suffix}"

        if len(simulated_single) > threshold and len(symbols) > min_symbols:
            # Multi-line wrap with trailing comments
            indent_match = re.match(r"^[ \t]*", match.group(1))
            base_indent = indent_match.group(0) if indent_match else ""
            item_indent = base_indent + "  "

            formatted_items = "\n".join(f"{item_indent}{sym},//" for sym in symbols)
            return f"{prefix} {{//\n{formatted_items}\n{base_indent}}} {suffix}"
        else:
            # Short enough or <= min_symbols: remove any internal comments / newlines
            if "\n" not in inside and "//" not in inside and "/*" not in inside:
                return full_match
            indent_match = re.match(r"^[ \t]*", match.group(1))
            base_indent = indent_match.group(0) if indent_match else ""
            return f"{base_indent}{prefix.strip()} {{{', '.join(symbols)}}} {suffix}"

    return IMPORT_RE.sub(replace_import, content)


def run_clang_format(content: str, assume_filename: str = "input.ts") -> str:
    """Runs clang-format on the given content string."""
    repo_root = Path(__file__).resolve().parents[4]
    clang_format_py = (
        repo_root / "third_party" / "depot_tools" / "clang_format.py"
    )
    try:
        res = subprocess.run(
            [
                sys.executable,
                str(clang_format_py),
                f"--assume-filename={assume_filename}",
            ],
            input=content.encode("utf-8"),
            capture_output=True,
            check=True,
        )
        return res.stdout.decode("utf-8").replace("\r\n", "\n")
    except (subprocess.SubprocessError, FileNotFoundError) as e:
        print(
            f"Warning: clang-format failed for {assume_filename}: {e}",
            file=sys.stderr,
        )
        return content


def process_file(
    path: Path,
    threshold: int,
    min_symbols: int,
    in_place: bool,
    check_only: bool,
) -> bool:
    """Processes a file. Returns True if the file needs / received formatting changes."""
    try:
        content = path.read_bytes().decode("utf-8").replace("\r\n", "\n")
    except Exception as e:
        print(f"Error reading {path}: {e}", file=sys.stderr)
        return False

    rewritten = rewrite_imports(
        content,
        threshold=threshold,
        min_symbols=min_symbols,
    )
    rewritten = run_clang_format(rewritten, assume_filename=str(path))

    needs_change = (rewritten != content)

    if check_only:
        return needs_change

    if in_place:
        if needs_change:
            path.write_bytes(rewritten.encode("utf-8"))
            print(f"Updated {path}")
        else:
            print(f"No changes for {path}")
    else:
        sys.stdout.buffer.write(rewritten.encode("utf-8"))

    return needs_change


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Rewrite long TypeScript imports to multi-line with trailing ',//' and format with clang-format."
    )
    parser.add_argument(
        "files",
        nargs="*",
        type=Path,
        help="Path to .ts files to process. If none provided, reads from stdin.",
    )
    parser.add_argument(
        "-i",
        "--in-place",
        action="store_true",
        help="Modify files in place.",
    )
    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Check whether files need formatting without modifying them. Exits with 1 if changes are needed.",
    )
    parser.add_argument(
        "-t",
        "--threshold",
        type=int,
        default=120,
        help="Line length threshold above which imports are rewritten (default: 120).",
    )
    parser.add_argument(
        "-s",
        "--min-symbols",
        type=int,
        default=2,
        help="Minimum number of symbols required to trigger multi-line wrapping (wrap if > min_symbols, default: 2).",
    )
    parser.add_argument(
        "--assume-filename",
        default="input.ts",
        help="Override filename used when running clang-format on stdin (default: input.ts).",
    )

    args = parser.parse_args()

    if not args.files or (len(args.files) == 1 and str(args.files[0]) == "-"):
        content = sys.stdin.buffer.read().decode("utf-8").replace("\r\n", "\n")
        rewritten = rewrite_imports(
            content,
            threshold=args.threshold,
            min_symbols=args.min_symbols,
        )
        rewritten = run_clang_format(rewritten, assume_filename=args.assume_filename)
        if args.check_only:
            if rewritten != content:
                sys.exit(1)
            sys.exit(0)
        sys.stdout.buffer.write(rewritten.encode("utf-8"))
    else:
        files_needing_formatting = []
        for file_path in args.files:
            if process_file(
                file_path,
                threshold=args.threshold,
                min_symbols=args.min_symbols,
                in_place=args.in_place,
                check_only=args.check_only,
            ):
                files_needing_formatting.append(file_path)

        if args.check_only and files_needing_formatting:
            sys.stderr.write("The following files need TypeScript import formatting:\n")
            for file_path in files_needing_formatting:
                sys.stderr.write(f"  {file_path}\n")
            sys.exit(1)


if __name__ == "__main__":
    main()
