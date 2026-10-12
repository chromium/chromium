#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that class member fields appear after member functions in class
definitions.

Per Google and Chromium C++ style guides, within each access section of class
definitions, declarations should generally be ordered:
  1. Types, aliases, static constants, friend declarations
  2. Member functions (constructors, destructors, methods, operators)
  3. Data members (member fields)

check_member_order.py <files>
"""

import argparse
import concurrent.futures
from dataclasses import dataclass, field
import os
import re
import sys
from typing import Dict, List, Optional


def strip_comments_and_strings(source: str) -> str:
  """Replaces comments and strings with spaces/newlines to preserve line
  numbers."""
  pattern = re.compile(
    r'(?P<raw_string>R"([a-zA-Z0-9_]*)\(.*?\)\2")|'
    r'(?P<string>"(?:\\.|[^"\\])*")|'
    r"(?P<char>'(?:\\.|[^'\\])*')|"
    r'(?P<block_comment>/\*.*?\*/)|'
    r'(?P<line_comment>//[^\n]*)',
    re.DOTALL,
  )

  def replacer(match):
    text = match.group(0)
    newline_count = text.count('\n')
    if match.group('block_comment') or match.group('line_comment'):
      return '\n' * newline_count + ' '
    elif match.group('raw_string') or match.group('string'):
      return '""' + '\n' * newline_count
    elif match.group('char'):
      return "''"
    return text

  return pattern.sub(replacer, source)


def strip_preprocessor_directives(source: str) -> str:
  """Replaces preprocessor directives with spaces/newlines to preserve line
  numbers."""
  pattern = re.compile(r'^[ \t]*#[^\n]*(?:\\\n[^\n]*)*', re.MULTILINE)

  def replacer(match):
    newline_count = match.group(0).count('\n')
    return '\n' * newline_count + ' '

  return pattern.sub(replacer, source)


@dataclass
class Member:
  # 'function', 'field', 'static_const', 'type', 'macro',
  # 'access_specifier', 'other'
  kind: str
  name: str
  line: int
  text: str
  access: str = 'private'


@dataclass
class Violation:
  class_name: str
  field_name: str
  field_line: int
  func_name: str
  func_line: int


@dataclass
class Scope:
  scope_type: str  # 'class', 'struct', 'enum', 'function', 'namespace', 'other'
  name: str = ""
  start_line: int = 0
  current_access: str = "private"
  members: List[Member] = field(default_factory=list)
  # Maps access specifier ('public', 'protected', 'private') to the first field
  # declared in that access level.
  first_field_by_access: Dict[str, Member] = field(default_factory=dict)
  violations: List[Violation] = field(default_factory=list)


RE_CLASS_HEADER = re.compile(
  r'\b(?P<kind>class|struct)\s+'
  r'(?:\[\[.*?\]\]\s+)?'
  r'(?:[A-Z0-9_]+_EXPORT\s+)?'
  r'(?P<name>[A-Za-z0-9_]+)'
  r'(?:\s+final)?'
  r'(?:\s*:[^{;]*)?$',
  re.DOTALL,
)

RE_ENUM_HEADER = re.compile(
  r'\benum\b(?:\s+(?:class|struct))?(?:\s+[A-Za-z0-9_]+)?(?:\s*:[^{;]*)?$',
  re.DOTALL,
)
RE_NAMESPACE_HEADER = re.compile(
  r'\bnamespace(?:\s+[A-Za-z0-9_:]+)?$', re.DOTALL
)
RE_ACCESS_SPECIFIER = re.compile(r'^\s*(public|protected|private)\s*:\s*$')
RE_STATIC_CONST = re.compile(r'\bstatic\s+(?:inline\s+)?(?:constexpr|const)\b')
RE_TYPE_OR_FRIEND = re.compile(r'^\s*(?:using\b|typedef\b|friend\b)')
RE_THREAD_ANNOTATION = re.compile(
  r'\b(?:[A-Z0-9_]*GUARDED_BY[A-Z0-9_]*|EXCLUSIVE_LOCKS_REQUIRED|'
  r'SHARED_LOCKS_REQUIRED|LOCKS_EXCLUDED|VALID_CONTEXT_REQUIRED)\s*\([^)]*\)'
)
RE_ALL_CAPS_MACRO = re.compile(r'^\s*([A-Z0-9_]+)\s*\([^)]*\)\s*;?$')

# Matches member field ending in _ (e.g. int foo_; raw_ptr<T> ptr_ = nullptr;)
RE_FIELD = re.compile(
  r'\b(?P<field_name>[a-z_][a-z0-9_]*_)\s*'
  r'(?:\[[^\]]*\]\s*)*'  # optional array brackets
  r'(?::\s*[0-9]+\s*)?'  # optional bitfield
  r'(?:GUARDED_BY\([^)]*\)\s*)?'
  r'(?:=\s*.*|\{[^}]*\}|\([^)]*\))?'  # optional initializer
  r';\s*$'
)

RE_MOCK_METHOD = re.compile(r'\bMOCK_METHOD(?:[0-9]+)?\s*\(')

RE_QUALIFIERS = re.compile(
  r'\s*(?:'
  r'const|'
  r'noexcept(?:\s*\([^)]*\))?|'
  r'override|'
  r'final|'
  r'=\s*(?:0|default|delete)|'
  r'&\s*|'
  r'&&\s*|'
  r'(?:GUARDED_BY|EXCLUSIVE_LOCKS_REQUIRED|LOCKS_EXCLUDED)\s*\([^)]*\)'
  r')\s*$'
)

ACCESS_ORDER = {'public': 0, 'protected': 1, 'private': 2}


def has_top_level_assignment(s: str) -> bool:
  """Returns True if '=' appears at depth 0, excluding '= 0', '= default',
  '= delete'."""
  depth = 0
  for i, ch in enumerate(s):
    if ch in '([{<':
      depth += 1
    elif ch in ')]}>':
      depth -= 1
    elif ch == '=' and depth == 0:
      rest = s[i + 1 :].strip()
      if rest in ('0', 'default', 'delete'):
        return False
      return True
  return False


def extract_func_decl(stmt: str) -> Optional[str]:
  """Extracts function name from a declaration ending in ';', or None."""
  s = stmt.strip()
  if not s.endswith(';'):
    return None
  s = s[:-1].strip()
  if has_top_level_assignment(s):
    return None

  while True:
    m = RE_QUALIFIERS.search(s)
    if m and m.start() < len(s):
      s = s[: m.start()].strip()
    else:
      break

  if not s.endswith(')'):
    return None

  depth = 0
  open_paren = -1
  for i in range(len(s) - 1, -1, -1):
    if s[i] == ')':
      depth += 1
    elif s[i] == '(':
      depth -= 1
      if depth == 0:
        open_paren = i
        break
  if open_paren == -1:
    return None

  params = s[open_paren + 1 : -1].strip()
  if re.fullmatch(
    r'(?:true|false|\d+|nullptr|NULL|"[^"]*"|\'[^\']*\')', params
  ):
    return None

  pre = s[:open_paren].strip()
  m_name = re.search(r'(\b~?[A-Za-z0-9_]+|\boperator\s*[^(\s]+)$', pre)
  if not m_name:
    return None
  name = m_name.group(1)
  if name.endswith('_') and not name.startswith('operator'):
    return None
  if name in ('if', 'for', 'while', 'switch', 'catch', 'static_assert'):
    return None
  return name


def extract_inline_func_name(stmt: str) -> Optional[str]:
  """Extracts function name from an inline function header before '{', or
  None."""
  s = stmt.strip()
  if not s:
    return None

  # Find colon for constructor initializer list at depth 0.
  depth = 0
  colon_pos = -1
  for i, ch in enumerate(s):
    if ch in '([{<':
      depth += 1
    elif ch in ')]}>':
      depth -= 1
    elif ch == ':' and depth == 0:
      if i + 1 < len(s) and s[i + 1] == ':':
        continue
      if i > 0 and s[i - 1] == ':':
        continue
      colon_pos = i
      break

  sig = s[:colon_pos].strip() if colon_pos != -1 else s.strip()
  sig = RE_THREAD_ANNOTATION.sub('', sig).strip()
  if has_top_level_assignment(sig):
    return None

  while True:
    m = RE_QUALIFIERS.search(sig)
    if m and m.start() < len(sig):
      sig = sig[: m.start()].strip()
    else:
      break

  if not sig.endswith(')'):
    return None

  depth = 0
  open_paren = -1
  for i in range(len(sig) - 1, -1, -1):
    if sig[i] == ')':
      depth += 1
    elif sig[i] == '(':
      depth -= 1
      if depth == 0:
        open_paren = i
        break
  if open_paren == -1:
    return None

  pre = sig[:open_paren].strip()
  m_name = re.search(r'(\b~?[A-Za-z0-9_]+|\boperator\s*[^(\s]+)$', pre)
  if not m_name:
    return None
  name = m_name.group(1)
  if name.endswith('_') and not name.startswith('operator'):
    return None
  if name in ('if', 'for', 'while', 'switch', 'catch', 'static_assert'):
    return None
  return name


def classify_statement(stmt: str, line: int, access: str) -> Optional[Member]:
  s = stmt.strip()
  if not s:
    return None

  if RE_ACCESS_SPECIFIER.match(s):
    m = RE_ACCESS_SPECIFIER.match(s)
    return Member(
      kind='access_specifier',
      name=m.group(1),
      line=line,
      text=s,
      access=m.group(1),
    )

  if RE_TYPE_OR_FRIEND.match(s):
    return Member(kind='type', name='', line=line, text=s, access=access)

  if RE_MOCK_METHOD.search(s):
    return Member(
      kind='function', name='MOCK_METHOD', line=line, text=s, access=access
    )

  m_macro = RE_ALL_CAPS_MACRO.match(s)
  if m_macro:
    return Member(
      kind='macro', name=m_macro.group(1), line=line, text=s, access=access
    )

  s_clean = RE_THREAD_ANNOTATION.sub('', s)

  func_name = extract_func_decl(s_clean)
  if func_name:
    return Member(
      kind='function', name=func_name, line=line, text=s, access=access
    )

  m_field = RE_FIELD.search(s_clean)
  if m_field:
    return Member(
      kind='field',
      name=m_field.group('field_name'),
      line=line,
      text=s,
      access=access,
    )

  if RE_STATIC_CONST.search(s):
    return Member(
      kind='static_const', name='', line=line, text=s, access=access
    )

  return Member(kind='other', name='', line=line, text=s, access=access)


def find_violations_in_content(content: str) -> List[Violation]:
  """Analyzes C++ content and returns a list of violations where member
  functions appear after member fields in class/struct definitions.
  """
  clean_text = strip_comments_and_strings(content)
  clean_text = strip_preprocessor_directives(clean_text)

  scope_stack: List[Scope] = []
  all_violations: List[Violation] = []

  stmt_buf = []
  stmt_start_line = 1
  stmt_started = False
  current_line = 1
  stmt_brace_depth = 0

  for ch in clean_text:
    if ch == '\n':
      current_line += 1

    if not stmt_started and not ch.isspace():
      stmt_start_line = current_line
      stmt_started = True

    if ch == '{':
      if stmt_brace_depth > 0:
        stmt_brace_depth += 1
        stmt_buf.append(ch)
        continue

      stmt = "".join(stmt_buf).strip()
      m_class = RE_CLASS_HEADER.search(stmt)
      m_enum = RE_ENUM_HEADER.search(stmt)
      m_ns = RE_NAMESPACE_HEADER.search(stmt)

      if m_enum:
        scope_stack.append(Scope(scope_type='enum', start_line=stmt_start_line))
      elif m_class and not RE_TYPE_OR_FRIEND.match(stmt):
        default_access = (
          "public" if m_class.group("kind") == "struct" else "private"
        )
        scope_stack.append(
          Scope(
            scope_type=m_class.group('kind'),
            name=m_class.group('name'),
            start_line=stmt_start_line,
            current_access=default_access,
          )
        )
      elif m_ns:
        scope_stack.append(
          Scope(scope_type='namespace', start_line=stmt_start_line)
        )
      else:
        if scope_stack and scope_stack[-1].scope_type in ('class', 'struct'):
          parent = scope_stack[-1]
          func_name = extract_inline_func_name(stmt)
          if func_name and not RE_ALL_CAPS_MACRO.match(stmt):
            func_member = Member(
              kind='function',
              name=func_name,
              line=stmt_start_line,
              text=stmt,
              access=parent.current_access,
            )
            parent.members.append(func_member)
            func_order = ACCESS_ORDER.get(parent.current_access, 2)
            for f_acc, f_mem in parent.first_field_by_access.items():
              if ACCESS_ORDER.get(f_acc, 2) >= func_order:
                v = Violation(
                  class_name=parent.name,
                  field_name=f_mem.name,
                  field_line=f_mem.line,
                  func_name=func_name,
                  func_line=stmt_start_line,
                )
                parent.violations.append(v)
                all_violations.append(v)
                break
            scope_stack.append(
              Scope(
                scope_type='function',
                name=func_name,
                start_line=stmt_start_line,
              )
            )
          else:
            stmt_brace_depth += 1
            stmt_buf.append(ch)
            continue
        else:
          scope_stack.append(
            Scope(scope_type='other', start_line=stmt_start_line)
          )

      stmt_buf = []
      stmt_started = False
      continue

    elif ch == '}':
      if stmt_brace_depth > 0:
        stmt_brace_depth -= 1
        stmt_buf.append(ch)
        continue

      if scope_stack:
        scope_stack.pop()
      stmt_buf = []
      stmt_started = False
      continue

    elif ch == ';':
      if stmt_brace_depth > 0:
        stmt_buf.append(ch)
        continue

      stmt_buf.append(ch)
      stmt = "".join(stmt_buf).strip()
      if scope_stack and scope_stack[-1].scope_type in ('class', 'struct'):
        parent = scope_stack[-1]
        member = classify_statement(
          stmt, stmt_start_line, parent.current_access
        )
        if member:
          parent.members.append(member)
          if member.kind == 'field':
            if parent.current_access not in parent.first_field_by_access:
              parent.first_field_by_access[parent.current_access] = member
          elif member.kind == 'function':
            func_order = ACCESS_ORDER.get(parent.current_access, 2)
            for f_acc, f_mem in parent.first_field_by_access.items():
              if ACCESS_ORDER.get(f_acc, 2) >= func_order:
                v = Violation(
                  class_name=parent.name,
                  field_name=f_mem.name,
                  field_line=f_mem.line,
                  func_name=member.name,
                  func_line=member.line,
                )
                parent.violations.append(v)
                all_violations.append(v)
                break
      stmt_buf = []
      stmt_started = False
      continue

    elif ch == ':':
      if stmt_brace_depth > 0:
        stmt_buf.append(ch)
        continue

      stmt_buf.append(ch)
      stmt = "".join(stmt_buf).strip()
      if RE_ACCESS_SPECIFIER.match(stmt):
        if scope_stack and scope_stack[-1].scope_type in ('class', 'struct'):
          parent = scope_stack[-1]
          member = classify_statement(
            stmt, stmt_start_line, parent.current_access
          )
          if member:
            parent.members.append(member)
            parent.current_access = member.name
        stmt_buf = []
        stmt_started = False
      continue

    stmt_buf.append(ch)

  return all_violations


def check_file(file_path: str) -> List[Violation]:
  """Checks a single file on disk and returns any violations found."""
  try:
    with open(file_path, 'r', encoding='utf-8') as f:
      content = f.read()
  except Exception as e:
    sys.stderr.write(f"Error reading {file_path}: {e}\n")
    return []
  return find_violations_in_content(content)


def main():
  parser = argparse.ArgumentParser(
    description=(
      "Check that class member fields appear after member functions in C++"
      " class definitions."
    )
  )
  parser.add_argument("files", nargs="+", help="Files to check.")
  args = parser.parse_args()

  files_with_violations: Dict[str, List[Violation]] = {}

  if len(args.files) == 1:
    viols = check_file(args.files[0])
    if viols:
      files_with_violations[args.files[0]] = viols
  else:
    max_workers = min(32, (os.cpu_count() or 1) + 4)
    with concurrent.futures.ThreadPoolExecutor(
      max_workers=max_workers
    ) as executor:
      future_to_file = {executor.submit(check_file, f): f for f in args.files}
      for future in concurrent.futures.as_completed(future_to_file):
        f = future_to_file[future]
        try:
          viols = future.result()
          if viols:
            files_with_violations[f] = viols
        except Exception as e:
          sys.stderr.write(f"Error checking {f}: {e}\n")

  if files_with_violations:
    for file_path in sorted(files_with_violations.keys()):
      viols = files_with_violations[file_path]
      for v in viols:
        sys.stderr.write(
          f"{file_path}:{v.func_line}: error: In class '{v.class_name}', "
          f"function '{v.func_name}' is declared after member field "
          f"'{v.field_name}' (line {v.field_line})\n"
        )
    sys.exit(1)

  sys.exit(0)


if __name__ == "__main__":
  main()
