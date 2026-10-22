#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that split Glic metrics are recorded correctly."""

import argparse
import json
import os
import re
import sys
import xml.etree.ElementTree as ET


def is_glic_cpp_source_file(path):
  """Returns True if path is a non-test C++ source file in Glic.

  Args:
    path: Relative or absolute path to the file.

  Returns:
    True if the file is a C++ source file and not a test or provider reporter.
  """
  path = path.replace('\\', '/')
  if not path.startswith('chrome/browser/glic/'):
    return False
  if not path.endswith(('.cc', '.h', '.mm')):
    return False
  if path.endswith(
    (
      '_test.cc',
      '_unittest.cc',
      '_browsertest.cc',
      '_interactive_uitest.cc',
      '_test.h',
      '_unittest.h',
      '_browsertest.h',
      '_interactive_uitest.h',
      '_test.mm',
      '_unittest.mm',
      '_browsertest.mm',
      '_interactive_uitest.mm',
    )
  ):
    return False
  if os.path.basename(path) == 'glic_provider_metrics.cc':
    return False
  return True


def CheckCppFiles(
  repo_root, histograms_xml_content, cpp_files=None, scan_all_glic_cpp=False
):
  """Checks C++ files for proper usage of Glic provider split metrics.

  Args:
    repo_root: Path to repository root.
    histograms_xml_content: String contents of Glic histograms.xml.
    cpp_files: Optional list of dicts with keys 'path', 'lines',
      'changed_lines', and 'is_affected'.
    scan_all_glic_cpp: When True, also scans all non-test C++ files under
      chrome/browser/glic on disk.

  Returns:
    List of error message strings.
  """
  if not histograms_xml_content and repo_root:
    histograms_xml_content = GetHistogramsXml(repo_root)

  split_base_names = []
  if histograms_xml_content:
    try:
      root = ET.fromstring(histograms_xml_content)
      for item in root.findall('.//histogram'):
        name = item.get('name')
        if name and name.startswith('Glic{GlicProvider}.'):
          base_name = name.replace('{GlicProvider}', '', 1)
          parts = re.split(r'(\{[^}]+\})', base_name)
          regex_parts = []
          prefix = ''
          found_token = False
          for part in parts:
            if not part:
              continue
            if part.startswith('{') and part.endswith('}'):
              regex_parts.append(r'[^"]*')
              found_token = True
            else:
              regex_parts.append(re.escape(part))
              if not found_token:
                prefix += part
          p = r'^' + ''.join(regex_parts) + r'$'
          split_base_names.append(
            (prefix if found_token else None, re.compile(p))
          )
    except ET.ParseError:
      pass

  all_cpp_files = list(cpp_files or [])

  if scan_all_glic_cpp and repo_root:
    glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
    if os.path.isdir(glic_dir):
      existing_paths = {f.get('path', '') for f in all_cpp_files}
      for root_dir, _, filenames in os.walk(glic_dir):
        for filename in filenames:
          abs_path = os.path.join(root_dir, filename)
          rel_path = os.path.relpath(abs_path, repo_root).replace('\\', '/')
          if rel_path in existing_paths:
            continue
          if not is_glic_cpp_source_file(rel_path):
            continue
          try:
            with open(abs_path, 'r', encoding='utf-8') as f:
              all_cpp_files.append(
                {
                  'path': rel_path,
                  'lines': f.read().splitlines(),
                  'changed_lines': [],
                  'is_affected': False,
                }
              )
          except IOError:
            pass

  uma_pattern = re.compile(
    r'\b(?:base::UmaHistogram\w*|UMA_HISTOGRAM_\w*)\s*\('
  )
  strcat_pattern = re.compile(r'\bbase::StrCat\s*\(\s*\{')

  def is_pattern_search(line_str):
    """Checks whether line matches UMA or base::StrCat invocation pattern."""
    return bool(uma_pattern.search(line_str) or strcat_pattern.search(line_str))

  errors = []
  for file_info in all_cpp_files:
    rel_path = file_info.get('path', '')
    lines = file_info.get('lines', [])
    changed_line_nums = set(file_info.get('changed_lines', []))
    is_affected = bool(file_info.get('is_affected', False))

    for idx, line in enumerate(lines):
      if re.match(r'^\s*//', line):
        continue

      is_raw_call = is_pattern_search(line)
      if not is_raw_call:
        continue

      literal = None
      spans_next_line = False
      literal_m = re.search(r'"(Glic\.[^"]+)"', line)
      if literal_m:
        literal = literal_m.group(1)
      elif idx + 1 < len(lines):
        next_line = lines[idx + 1]
        literal_m = re.search(r'^\s*"(Glic\.[^"]+)"', next_line)
        if literal_m:
          literal = literal_m.group(1)
          spans_next_line = True

      matched = False
      if literal:
        for prefix, regex in split_base_names:
          is_strcat = bool(strcat_pattern.search(line))
          if regex.match(literal) or (
            is_strcat and prefix and literal == prefix
          ):
            matched = True
            break

      if matched:
        msg = (
          f"{rel_path}:{idx + 1}: '{literal}' is split by provider; "
          "record it via GlicMetricReporter "
          "(chrome/browser/glic/public/glic_provider_metrics.h)."
        )
        errors.append(msg)
        continue

      # Raw UMA // Justification: check ONLY on modified lines.
      if is_affected and uma_pattern.search(line):
        if (idx + 1) in changed_line_nums or (
          spans_next_line and (idx + 2) in changed_line_nums
        ):
          j = idx - 1
          comments = []
          while j >= 0 and re.match(r'^\s*//', lines[j]):
            comments.append(lines[j])
            j -= 1
          inline_comment_m = re.search(r'//(.*)$', line)
          if inline_comment_m:
            comments.append('//' + inline_comment_m.group(1))

          found_justification = False
          for c in comments:
            if re.match(r'^\s*//\s*Justification:\s*\S', c):
              found_justification = True
              break

          if not found_justification:
            msg = (
              f"{rel_path}:{idx + 1}: Direct base::UmaHistogram*/"
              "UMA_HISTOGRAM_* call in chrome/browser/glic requires a "
              "'// Justification: <reason>' comment immediately above "
              "explaining why GlicMetricReporter cannot be used."
            )
            errors.append(msg)

  return errors


def _ImportHistogramValidation(repo_root):
  """Imports histogram_validation via the tools/metrics setup_modules shim."""
  import sys

  histograms_dir = os.path.join(repo_root, 'tools', 'metrics', 'histograms')
  if histograms_dir not in sys.path:
    sys.path.insert(0, histograms_dir)
  import setup_modules  # noqa: F401  # pylint: disable=unused-import
  from chromium_src.tools.metrics.histograms import (
    histogram_validation,
  )

  return histogram_validation


def CheckHistogramsXml(
    repo_root, old_histograms_content, new_histograms_content
):
  """Checks that newly added Glic histograms are split by provider.

  Args:
    repo_root: Path to repository root.
    old_histograms_content: String contents of previous histograms.xml.
    new_histograms_content: String contents of new histograms.xml.

  Returns:
    List of error message strings.
  """
  errors = []
  if not new_histograms_content:
    return errors

  if not repo_root:
    return ["Could not find repo_root"]

  histogram_validation = _ImportHistogramValidation(repo_root)
  from chromium_src.tools.metrics.histograms import (
    histogram_paths,
  )
  import chromium_src.tools.metrics.histograms.merge_xml as merge_xml

  old_contents = (
    old_histograms_content.splitlines() if old_histograms_content else []
  )
  new_contents = (
    new_histograms_content.splitlines() if new_histograms_content else []
  )

  variants_paths = [
    os.path.join(repo_root, p) for p in histogram_paths._VARIANTS_XML_RELATIVE
  ]
  variants_trees = []
  for vp in variants_paths:
    if os.path.exists(vp):
      with open(vp, 'r', encoding='utf-8') as f:
        try:
          variants_trees.append(ET.fromstring(f.read()))
        except ET.ParseError:
          pass
  if variants_trees:
    variants_doc = merge_xml.MergeTrees(
      variants_trees, should_expand_owners=False
    )
  else:
    variants_doc = ET.fromstring(
      '<variants-configuration></variants-configuration>'
    )

  old_names = histogram_validation.get_histogram_names(
    [old_contents], variants_doc
  )
  new_names = histogram_validation.get_histogram_names(
    [new_contents], variants_doc
  )

  added_names = new_names - old_names

  try:
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    root = ET.fromstring(new_histograms_content, parser=parser)
  except ET.ParseError:
    return errors

  new_glic_histograms = []
  for item in root.findall('.//histogram'):
    unexpanded_name = item.get('name')
    if unexpanded_name and unexpanded_name.startswith('Glic'):
      new_glic_histograms.append(item)

  for name in added_names:
    if not name.startswith('Glic.'):
      continue
    if name.startswith('Glic.Consumer.') or name.startswith('Glic.Enterprise.'):
      continue

    rest = name[len('Glic.') :]
    has_consumer = f'Glic.Consumer.{rest}' in new_names
    has_enterprise = f'Glic.Enterprise.{rest}' in new_names

    if has_consumer and has_enterprise:
      continue

    source_item = None
    source_unexpanded_name = None
    for item in new_glic_histograms:
      unexpanded_name = item.get('name')
      parts = re.split(r'\{[^}]+\}', unexpanded_name)
      regex = '^' + '.*'.join(re.escape(p) for p in parts) + '$'
      if re.match(regex, name):
        source_item = item
        source_unexpanded_name = unexpanded_name
        break

    if source_item is not None:
      allowed = False
      has_unsplittable_comment = False
      for child in source_item:
        if not isinstance(child.tag, str):
          if child.text and 'Allowed unsplittable:' in child.text:
            has_unsplittable_comment = True
            parts = child.text.split('Allowed unsplittable:', 1)
            if len(parts) > 1:
              reason = parts[1].strip()
              if reason:
                allowed = True
              else:
                msg = (
                  f"Histogram '{source_unexpanded_name}' has "
                  "'Allowed unsplittable:' comment but missing reason."
                )
                if msg not in errors:
                  errors.append(msg)

      if not allowed and not has_unsplittable_comment:
        msg = (
          f"Added Glic histogram '{name}' is not split by provider: "
          f"use the Glic{{GlicProvider}}. prefix or add "
          f"<!-- Allowed unsplittable: <reason> --> inside its "
          f"<histogram>."
        )
        if msg not in errors:
          errors.append(msg)

  return errors


def find_repo_root(start_path=None):
  """Finds the repository root containing .gn starting from start_path."""
  if not start_path:
    start_path = os.path.dirname(os.path.abspath(__file__))
  path = os.path.abspath(start_path)
  while True:
    if os.path.exists(os.path.join(path, '.gn')):
      return path
    parent = os.path.dirname(path)
    if parent == path:
      break
    path = parent
  cwd = os.path.abspath(os.getcwd())
  while True:
    if os.path.exists(os.path.join(cwd, '.gn')):
      return cwd
    parent = os.path.dirname(cwd)
    if parent == cwd:
      return None
    cwd = parent


def GetHistogramsXml(repo_root, affected_files=None):
  """Returns the contents of the Glic histograms.xml.

  Args:
    repo_root: Path to the repository root.
    affected_files: Optional list of affected-file dicts from the presubmit
      payload. If histograms.xml is among them, its new contents are used.

  Returns:
    The histograms.xml contents, or '' if it cannot be read.
  """
  if affected_files:
    for f in affected_files:
      if f.get('path', '').endswith(
        'tools/metrics/histograms/metadata/glic/histograms.xml'
      ):
        return '\n'.join(f.get('new_contents', []))

  h_path = os.path.join(
    repo_root,
    'tools',
    'metrics',
    'histograms',
    'metadata',
    'glic',
    'histograms.xml',
  )
  if os.path.exists(h_path):
    try:
      with open(h_path, 'r', encoding='utf-8') as f:
        return f.read()
    except IOError:
      pass
  return ''


def main(argv=None):
  """Runs the Glic metrics checks.

  With --presubmit-json, reads the presubmit payload from stdin and always
  prints a JSON list of error strings to stdout. Otherwise, checks the given
  files (or all of chrome/browser/glic) and prints errors to stderr.

  Args:
    argv: Command-line arguments, excluding the program name.

  Returns:
    0 if no errors were found, 1 otherwise.
  """
  if argv is None:
    argv = sys.argv[1:]

  parser = argparse.ArgumentParser(description='Check Glic metrics')
  parser.add_argument(
    '--presubmit-json',
    action='store_true',
    help='Read payload JSON from stdin and output result JSON to stdout',
  )
  parser.add_argument('--repo-root', help='Path to chromium repo root')
  parser.add_argument('files', nargs='*', help='Specific files to check')
  args = parser.parse_args(argv)

  repo_root = args.repo_root or find_repo_root()
  if not repo_root:
    if args.presubmit_json:
      print(json.dumps(['Could not find repository root']))
    else:
      print('Could not find repository root', file=sys.stderr)
    return 1

  if args.presubmit_json:
    try:
      data = json.load(sys.stdin)
    except Exception as e:
      print(f'Error reading JSON from stdin: {e}', file=sys.stderr)
      return 1

    checks = data.get('checks', [])
    affected_files = data.get('affected_files', [])

    errors = []

    # Trigger detection
    trigger_suffixes = (
      'tools/metrics/histograms/metadata/glic/histograms.xml',
      'chrome/browser/glic/PRESUBMIT.py',
      'chrome/browser/glic/tools/check_glic_metrics.py',
    )
    scan_all_glic_cpp = any(
      f.get('path', '').endswith(t)
      for f in affected_files
      for t in trigger_suffixes
    )

    if 'cpp' in checks:
      cpp_files = []
      for f in affected_files:
        if is_glic_cpp_source_file(f.get('path', '')):
          cpp_files.append(
            {
              'path': f.get('path', ''),
              'lines': f.get('new_contents', []),
              'changed_lines': f.get('changed_lines', []),
              'is_affected': True,
            }
          )
      if cpp_files or scan_all_glic_cpp:
        histograms_xml = GetHistogramsXml(repo_root, affected_files)
        errors.extend(
          CheckCppFiles(repo_root, histograms_xml, cpp_files, scan_all_glic_cpp)
        )

    if 'xml' in checks:
      for f in affected_files:
        if f.get('path', '').endswith(
          'tools/metrics/histograms/metadata/glic/histograms.xml'
        ):
          old_xml = '\n'.join(f.get('old_contents', []))
          new_xml = '\n'.join(f.get('new_contents', []))
          errors.extend(CheckHistogramsXml(repo_root, old_xml, new_xml))
          break

    print(json.dumps(errors))
    return 1 if errors else 0

  # CLI mode
  histograms_xml = GetHistogramsXml(repo_root, None)
  if args.files:
    cpp_files = []
    for f_path in args.files:
      abs_path = os.path.abspath(f_path)
      rel_path = os.path.relpath(abs_path, repo_root).replace('\\', '/')
      try:
        with open(abs_path, 'r', encoding='utf-8') as f:
          lines = f.read().splitlines()
      except IOError:
        lines = []
      cpp_files.append(
        {
          'path': rel_path,
          'lines': lines,
          'changed_lines': [],
          'is_affected': False,
        }
      )
    errors = CheckCppFiles(
      repo_root, histograms_xml, cpp_files=cpp_files, scan_all_glic_cpp=False
    )
  else:
    errors = CheckCppFiles(
      repo_root, histograms_xml, cpp_files=[], scan_all_glic_cpp=True
    )

  if errors:
    for err in errors:
      print(err, file=sys.stderr)
    return 1

  print('All Glic C++ files passed split-metric check.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
