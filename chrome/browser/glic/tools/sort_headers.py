#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Attempts to sort header includes in C++ files.

sort_headers.py [--check-only] <files>

Reorganizes preprocessor #if blocks, preserves comments/barriers,
and delegates header sorting, classification, and regrouping to clang-format.
"""

import argparse
import concurrent.futures
from dataclasses import dataclass, field
import os
import re
import shutil
import subprocess
import sys

_INCLUDE_RE = re.compile(r'^\s*#(include|import)\s+([<"].+?[">])\s*(?://.*)?$')
_IF_RE = re.compile(r'^\s*#\s*if\s+(.+)$')
_IFDEF_RE = re.compile(r'^\s*#\s*ifdef\s+(.+)$')
_IFNDEF_RE = re.compile(r'^\s*#\s*ifndef\s+(.+)$')
_ELIF_RE = re.compile(r'^\s*#\s*elif\s+(.+)$')
_ELSE_RE = re.compile(r'^\s*#\s*else(\s*(?://.*|/\*.*\*/\s*))?$')
_ENDIF_RE = re.compile(r'^\s*#\s*endif(\s*(?://.*|/\*.*\*/\s*))?$')
_DEFINE_RE = re.compile(r'^\s*#\s*define\s+([A-Za-z0-9_]+)')
_COMMENT_RE = re.compile(r'^\s*(?://.*|/\*.*\*/\s*)$')
_BLANK_RE = re.compile(r'^\s*$')

_INLINE_COMMENT_RE = re.compile(r'^(.*?)(?:\s*(//.*|/\*.*\*/\s*))?$')


@dataclass(eq=False)
class IncludeItem:
    raw_lines: list[str]
    header_path: str
    end_line_idx: int = 0


@dataclass(eq=False)
class CommentItem:
    text: str
    end_line_idx: int = 0


@dataclass(eq=False)
class GuardedBranch:
    type: str  # 'IF', 'ELIF', 'ELSE'
    raw_cond: str
    full_cond: str
    outer_cond: str = ""
    includes: list[IncludeItem] = field(default_factory=list)
    preamble: list[str] = field(default_factory=list)
    comment: str = ""


@dataclass(eq=False)
class GuardedChain:
    branches: list[GuardedBranch] = field(default_factory=list)
    endif_comment: str = ""
    start_line_idx: int = 0
    end_line_idx: int = 0


@dataclass(eq=False)
class IncludeGroup:
    unguarded_includes: list[IncludeItem] = field(default_factory=list)
    guarded_chains: list[GuardedChain] = field(default_factory=list)

    @property
    def end_line_idx(self) -> int:
        max_idx = 0
        if self.unguarded_includes:
            max_idx = max(max_idx, self.unguarded_includes[-1].end_line_idx)
        for chain in self.guarded_chains:
            if chain.end_line_idx > max_idx:
                max_idx = chain.end_line_idx
        return max_idx


@dataclass(eq=False)
class IfStackFrame:
    type: str
    raw_cond: str
    accum_neg: list[str]
    full_cond: str
    outer_cond: str
    chain_obj: GuardedChain
    branch: GuardedBranch


@dataclass
class ParseState:
    include_lines: list[str]
    i: int = 0
    items: list[IncludeGroup | CommentItem] = field(default_factory=list)
    current_group: IncludeGroup = field(default_factory=IncludeGroup)
    buffered_preamble: list[str] = field(default_factory=list)
    if_stack: list[IfStackFrame] = field(default_factory=list)
    last_was_blank: bool = False

    @property
    def line(self) -> str:
        return self.include_lines[self.i]

    def match(self, pattern: re.Pattern) -> re.Match | None:
        """Matches pattern against current line and
           advances index on success."""
        if self.i >= len(self.include_lines):
            return None
        m = pattern.match(self.line)
        if m:
            self.i += 1
        return m

    def get_current_outer_cond(self) -> str:
        return combine_conditions([frame.full_cond for frame in self.if_stack])

    def consume_preamble(self) -> list[str]:
        """Consumes and clears any buffered preamble comment lines."""
        pre = self.buffered_preamble
        self.buffered_preamble = []
        return pre

    def emit_comment_barrier(self, comment_lines: list[str]):
        """Emits a standalone comment barrier item."""
        if not self.if_stack:
            if (self.current_group.unguarded_includes
                    or self.current_group.guarded_chains):
                self.items.append(self.current_group)
                self.current_group = IncludeGroup()
            self.items.append(
                CommentItem(text="".join(comment_lines), end_line_idx=self.i))
        else:
            self.buffered_preamble.extend(comment_lines)


def split_cond_and_comment(line_body: str) -> tuple[str, str]:
    m = _INLINE_COMMENT_RE.match(line_body.strip())
    if m and m.group(2):
        cond = m.group(1).strip()
        comment = f"  {m.group(2)}"
        return cond, comment
    return line_body.strip(), ""


def negate_condition(cond: str) -> str:
    cond = cond.strip()
    if cond.startswith('!') and not ('&&' in cond or '||' in cond):
        return cond[1:].strip()
    if (cond.startswith('defined(') or cond.startswith('BUILDFLAG(')
            or cond.startswith('HAS_') or cond.startswith('USES_')):
        return f"!{cond}"
    if not ('&&' in cond or '||' in cond):
        return f"!{cond}"
    return f"!({cond})"


def combine_conditions(cond_list: list[str]) -> str:
    cleaned = [c.strip() for c in cond_list if c.strip()]
    if not cleaned:
        return ""
    if len(cleaned) == 1:
        return cleaned[0]
    parts = [f"({c})" if '||' in c else c for c in cleaned]
    return " && ".join(parts)


def find_clang_format() -> str:
    for name in ['clang-format', 'clang-format.bat', 'clang-format.exe']:
        path = shutil.which(name)
        if path:
            return path

    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(
        os.path.join(script_dir, '..', '..', '..', '..'))

    candidates = [
        os.path.join(repo_root, 'buildtools', 'win', 'clang-format.exe'),
        os.path.join(repo_root, 'buildtools', 'mac', 'clang-format'),
        os.path.join(repo_root, 'buildtools', 'linux64', 'clang-format'),
        os.path.join(repo_root, 'third_party', 'depot_tools',
                     'clang-format.bat'),
        os.path.join(repo_root, 'third_party', 'depot_tools', 'clang-format'),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c

    return 'clang-format'


def run_clang_format(content: str, file_path: str) -> str:
    try:
        clang_bin = find_clang_format()
        use_shell = sys.platform == 'win32' and (clang_bin.endswith('.bat')
                                                 or clang_bin.endswith('.cmd'))

        p = subprocess.Popen(
            [clang_bin, f'--assume-filename={file_path}'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            shell=use_shell,
        )
        out, err = p.communicate(content)
        if p.returncode == 0 and out:
            return out
        if err:
            sys.stderr.write(f"clang-format error: {err}\n")
    except Exception as e:
        sys.stderr.write(f"clang-format execution error: {e}\n")
    return content


def parse_blank(state: ParseState) -> bool:
    if not state.match(_BLANK_RE):
        return False
    state.last_was_blank = True
    return True


def parse_comment(state: ParseState) -> bool:
    if not _COMMENT_RE.match(state.line):
        return False

    state.last_was_blank = False

    start = state.i
    while state.i < len(state.include_lines) and _COMMENT_RE.match(
            state.line) and not _BLANK_RE.match(state.line):
        state.i += 1

    comment_block = state.include_lines[start:state.i]
    state.emit_comment_barrier(comment_block)
    return True


def parse_include(state: ParseState) -> bool:
    line = state.line
    m_inc = state.match(_INCLUDE_RE)
    if not m_inc:
        return False

    raw_lines = state.consume_preamble() + [line]
    inc_item = IncludeItem(raw_lines, m_inc.group(2), end_line_idx=state.i)

    if not state.if_stack:
        state.current_group.unguarded_includes.append(inc_item)
    else:
        state.if_stack[-1].branch.includes.append(inc_item)
    state.last_was_blank = False
    return True


def parse_if(state: ParseState) -> bool:
    m_if = state.match(_IF_RE)
    m_ifdef = None if m_if else state.match(_IFDEF_RE)
    m_ifndef = None if (m_if or m_ifdef) else state.match(_IFNDEF_RE)
    if not (m_if or m_ifdef or m_ifndef):
        return False

    pre_lines = state.consume_preamble()

    if m_if:
        raw_cond, branch_comment = split_cond_and_comment(m_if.group(1))
    elif m_ifdef:
        def_name, branch_comment = split_cond_and_comment(m_ifdef.group(1))
        raw_cond = f"defined({def_name})"
    else:
        def_name, branch_comment = split_cond_and_comment(m_ifndef.group(1))
        raw_cond = f"!defined({def_name})"

    outer_cond = state.get_current_outer_cond()
    full_cond = combine_conditions([outer_cond, raw_cond
                                    ]) if outer_cond else raw_cond

    branch = GuardedBranch(type='IF',
                           raw_cond=raw_cond,
                           full_cond=full_cond,
                           outer_cond=outer_cond,
                           preamble=pre_lines,
                           comment=branch_comment)
    chain = GuardedChain(branches=[branch], start_line_idx=state.i)
    state.current_group.guarded_chains.append(chain)

    frame = IfStackFrame(
        type='IF',
        raw_cond=raw_cond,
        accum_neg=[],
        full_cond=full_cond,
        outer_cond=outer_cond,
        chain_obj=chain,
        branch=branch,
    )
    state.if_stack.append(frame)
    state.last_was_blank = False
    return True


def parse_elif(state: ParseState) -> bool:
    if not state.if_stack:
        return False
    m_elif = state.match(_ELIF_RE)
    if not m_elif:
        return False

    pre_lines = state.consume_preamble()
    raw_cond, branch_comment = split_cond_and_comment(m_elif.group(1))
    top = state.if_stack.pop()
    neg_prev = negate_condition(top.raw_cond)
    new_accum = top.accum_neg + [neg_prev]
    branch_cond = combine_conditions(new_accum + [raw_cond])
    outer_cond = top.outer_cond
    full_cond = combine_conditions([outer_cond, branch_cond
                                    ]) if outer_cond else branch_cond

    branch = GuardedBranch(type='ELIF',
                           raw_cond=raw_cond,
                           full_cond=full_cond,
                           outer_cond=outer_cond,
                           preamble=pre_lines,
                           comment=branch_comment)
    top.chain_obj.branches.append(branch)

    frame = IfStackFrame(
        type='ELIF',
        raw_cond=raw_cond,
        accum_neg=new_accum,
        full_cond=full_cond,
        outer_cond=outer_cond,
        chain_obj=top.chain_obj,
        branch=branch,
    )
    state.if_stack.append(frame)
    state.last_was_blank = False
    return True


def parse_else(state: ParseState) -> bool:
    if not state.if_stack:
        return False
    m_else = state.match(_ELSE_RE)
    if not m_else:
        return False

    pre_lines = state.consume_preamble()
    else_comment = m_else.group(1) or ""
    top = state.if_stack.pop()
    neg_prev = negate_condition(top.raw_cond)
    new_accum = top.accum_neg + [neg_prev]
    branch_cond = combine_conditions(new_accum)
    outer_cond = top.outer_cond
    full_cond = combine_conditions([outer_cond, branch_cond
                                    ]) if outer_cond else branch_cond

    branch = GuardedBranch(type='ELSE',
                           raw_cond='',
                           full_cond=full_cond,
                           outer_cond=outer_cond,
                           preamble=pre_lines,
                           comment=else_comment)
    top.chain_obj.branches.append(branch)

    frame = IfStackFrame(
        type='ELSE',
        raw_cond='',
        accum_neg=new_accum,
        full_cond=full_cond,
        outer_cond=outer_cond,
        chain_obj=top.chain_obj,
        branch=branch,
    )
    state.if_stack.append(frame)
    state.last_was_blank = False
    return True


def parse_endif(state: ParseState) -> bool:
    if not state.if_stack:
        return False
    m_endif = state.match(_ENDIF_RE)
    if not m_endif:
        return False

    state.consume_preamble()
    top = state.if_stack.pop()
    top.chain_obj.endif_comment = m_endif.group(1) or ""
    top.chain_obj.end_line_idx = state.i
    state.last_was_blank = False
    return True


def parse_file_includes(
    lines: list[str]
) -> tuple[list[str], list[IncludeGroup | CommentItem], list[str]]:
    """Parses a file's lines into (preamble, include_items, postscript)."""
    header_guard_name = None
    first_inc = -1

    for idx, line in enumerate(lines):
        if header_guard_name is None:
            m_if = _IFNDEF_RE.match(line)
            if m_if:
                header_guard_name = m_if.group(1).strip()
        if _INCLUDE_RE.match(line):
            first_inc = idx
            break

    if first_inc == -1:
        return lines, [], []

    section_start = first_inc
    while section_start > 0:
        prev = lines[section_start - 1]
        if _COMMENT_RE.match(prev) and not _BLANK_RE.match(prev):
            if section_start > 1 and _BLANK_RE.match(lines[section_start - 2]):
                section_start -= 1
                break
            section_start -= 1
        elif _IF_RE.match(prev) or _IFDEF_RE.match(prev):
            section_start -= 1
        elif _IFNDEF_RE.match(prev):
            m_if = _IFNDEF_RE.match(prev)
            if header_guard_name and m_if.group(
                    1).strip() == header_guard_name:
                break
            section_start -= 1
        else:
            break

    preamble = lines[:section_start]
    state = ParseState(include_lines=lines, i=section_start)

    while state.i < len(state.include_lines):
        if (parse_blank(state) or parse_comment(state) or parse_include(state)
                or parse_if(state) or parse_elif(state) or parse_else(state)
                or parse_endif(state)):
            continue

        # Unrecognized line (code, namespace, code #if, etc.) ->
        #   stop include section
        break

    if state.if_stack:
        root_unclosed = state.if_stack[0].chain_obj
        unparsed_start = root_unclosed.start_line_idx
        state.if_stack.clear()
        state.current_group.guarded_chains = [
            c for c in state.current_group.guarded_chains
            if c.start_line_idx < unparsed_start and c.end_line_idx > 0
        ]
        unparsed_cutoff = unparsed_start
    else:
        unparsed_cutoff = None

    if (state.current_group.unguarded_includes
            or state.current_group.guarded_chains):
        state.items.append(state.current_group)

    valid_items = []
    for item in state.items:
        if isinstance(item, IncludeGroup):
            item.guarded_chains = [
                c for c in item.guarded_chains if any(
                    len(b.includes) > 0 for b in c.branches)
            ]
            if item.unguarded_includes or item.guarded_chains:
                valid_items.append(item)
        elif isinstance(item, CommentItem):
            if (unparsed_cutoff is None
                    or item.end_line_idx <= unparsed_cutoff):
                valid_items.append(item)

    while valid_items and isinstance(valid_items[-1], CommentItem):
        valid_items.pop()

    if valid_items:
        last_end = max(
            (it.end_line_idx for it in valid_items if it.end_line_idx > 0),
            default=section_start,
        )
        if unparsed_cutoff is not None:
            last_end = min(last_end, unparsed_cutoff)
        postscript = lines[last_end:]
    else:
        postscript = lines[section_start:]

    return preamble, valid_items, postscript


def format_inc_list(inc_list: list[IncludeItem]) -> list[str]:
    out = []
    sorted_incs = sorted(inc_list, key=lambda item: item.header_path)
    for inc in sorted_incs:
        out.extend(inc.raw_lines)
    return out


def format_include_group(group: IncludeGroup) -> list[list[str]]:
    """Formats an IncludeGroup into blocks of line lists."""
    cond_counts = {}
    for chain in group.guarded_chains:
        for b in chain.branches:
            if b.includes:
                cond_counts[b.full_cond] = cond_counts.get(b.full_cond, 0) + 1

    chain_can_preserve = {}
    for chain in group.guarded_chains:
        can = True
        for b in chain.branches:
            if b.outer_cond != '' or (b.includes
                                      and cond_counts.get(b.full_cond, 0) > 1):
                can = False
                break
        chain_can_preserve[chain] = can

    formatted_guarded_blocks = []
    handled_chains = set()

    for chain in group.guarded_chains:
        if chain in handled_chains:
            continue
        if chain_can_preserve[chain]:
            handled_chains.add(chain)
            lines_chain = []
            has_any_inc = False
            for b in chain.branches:
                if b.includes:
                    has_any_inc = True
                    lines_chain.extend(b.preamble)
                    if b.type == 'IF':
                        lines_chain.append(f"#if {b.raw_cond}{b.comment}\n")
                    elif b.type == 'ELIF':
                        lines_chain.append(f"#elif {b.raw_cond}{b.comment}\n")
                    elif b.type == 'ELSE':
                        lines_chain.append(f"#else{b.comment}\n")
                    lines_chain.extend(format_inc_list(b.includes))
            if has_any_inc:
                lines_chain.append(f"#endif{chain.endif_comment}\n")
                formatted_guarded_blocks.append(lines_chain)

    unhandled_branches_by_cond = {}
    cond_order = []
    cond_comment_map = {}

    for chain in group.guarded_chains:
        if chain not in handled_chains:
            for b in chain.branches:
                if b.includes:
                    cond = b.full_cond
                    if cond not in unhandled_branches_by_cond:
                        unhandled_branches_by_cond[cond] = []
                        cond_order.append(cond)
                        cond_comment_map[cond] = chain.endif_comment
                    unhandled_branches_by_cond[cond].extend(b.includes)

    for cond in cond_order:
        branch_incs = unhandled_branches_by_cond[cond]
        if branch_incs:
            lines_b = [f"#if {cond}\n"]
            lines_b.extend(format_inc_list(branch_incs))
            lines_b.append(f"#endif{cond_comment_map.get(cond, '')}\n")
            formatted_guarded_blocks.append(lines_b)

    blocks_to_output = []
    if group.unguarded_includes:
        blocks_to_output.append(format_inc_list(group.unguarded_includes))

    for gb in formatted_guarded_blocks:
        blocks_to_output.append(gb)

    return blocks_to_output


def sort_file_content(content: str, file_path: str) -> str:
    lines = content.splitlines(keepends=True)
    preamble, items, postscript = parse_file_includes(lines)

    if not items:
        return content

    include_section_lines = []
    for item_idx, item in enumerate(items):
        if isinstance(item, CommentItem):
            if item_idx > 0 and (not include_section_lines
                                 or include_section_lines[-1] != "\n"):
                include_section_lines.append("\n")
            include_section_lines.append(item.text)
            continue

        blocks_to_output = format_include_group(item)

        prev_was_comment = (item_idx > 0
                            and isinstance(items[item_idx - 1], CommentItem))

        for b_idx, block_lines in enumerate(blocks_to_output):
            if b_idx > 0 or (item_idx > 0 and not prev_was_comment
                             and include_section_lines
                             and include_section_lines[-1] != "\n"):
                include_section_lines.append("\n")
            include_section_lines.extend(block_lines)

    raw_include_content = "".join(include_section_lines)
    formatted_includes = run_clang_format(raw_include_content, file_path)
    return "".join(preamble) + formatted_includes + "".join(postscript)


def process_file(file_path: str, check_only: bool) -> bool:
    """Processes a single file.
       Returns True if file needs changes / was modified."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            original = f.read()
    except Exception as e:
        sys.stderr.write(f"Error reading {file_path}: {e}\n")
        return False

    sorted_content = sort_file_content(original, file_path)
    needs_change = (sorted_content != original)

    if needs_change and not check_only:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(sorted_content)

    return needs_change


def main():
    parser = argparse.ArgumentParser(
        description="Sort header includes in C++ files.")
    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Check include order without modifying files.",
    )
    parser.add_argument("files", nargs="+", help="Files to process.")
    args = parser.parse_args()

    modified_files = []
    if len(args.files) == 1:
        if process_file(args.files[0], args.check_only):
            modified_files.append(args.files[0])
    else:
        max_workers = min(32, (os.cpu_count() or 1) + 4)
        with concurrent.futures.ThreadPoolExecutor(
                max_workers=max_workers) as executor:
            future_to_file = {
                executor.submit(process_file, f, args.check_only): f
                for f in args.files
            }
            for future in concurrent.futures.as_completed(future_to_file):
                f = future_to_file[future]
                try:
                    if future.result():
                        modified_files.append(f)
                except Exception as e:
                    sys.stderr.write(f"Error processing {f}: {e}\n")

    if modified_files:
        if args.check_only:
            sys.stderr.write("Error: Incorrect header order in files:\n")
            for file_path in sorted(modified_files):
                sys.stdout.write(f"{file_path}\n")
            sys.exit(1)
        else:
            sys.stdout.write("Sorting includes in files:\n")
            for file_path in sorted(modified_files):
                sys.stdout.write(f"{file_path}\n")
            sys.exit(0)

    sys.exit(0)


if __name__ == "__main__":
    main()
