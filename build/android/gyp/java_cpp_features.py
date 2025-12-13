#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
import sys
import zipfile

from util import build_utils  # pylint: disable=unused-import
from util import java_cpp_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


class FeatureParserDelegate(java_cpp_utils.CppConstantParser.Delegate):
  # Ex. 'BASE_FEATURE(kConstantName, "StringNameOfTheFeature", ...);'
  # or 'BASE_FEATURE(kConstantName, ...);'
  # would parse as:
  #   ExtractConstantName() -> 'ConstantName'
  #   ExtractValue() -> '"StringNameOfTheFeature"' or '"ConstantName"'
  _FEATURE_RE = re.compile(r'BASE_FEATURE\(\s*(k\w+),')
  _STRING_LITERAL_RE = re.compile(r'"(?:\\"|[^"])*"')
  _constant_name = None  # The name of the current macro.
  _comma_count = 0  # Number of commas seen in the current macro.
  _string_literals = []  # All string literals seen in the current macro.

  def ExtractConstantName(self, line):
    # Reset to a clean state when extracting constant name.
    self._comma_count = 0
    self._string_literals = []
    self._constant_name = None

    match = self._FEATURE_RE.match(line)
    if match:
      # The regex ensures that the feature name starts with 'k'.
      feature_name = match.group(1)
      self._constant_name = feature_name[1:]
      return self._constant_name
    return None

  def ExtractValue(self, line):
    if line.strip().startswith('#') or line.strip().startswith("//"):
      # Skip C++ directives and comments within macros
      return None

    # Track all string literals.
    self._string_literals.extend(self._STRING_LITERAL_RE.findall(line))

    self._comma_count += line.count(',')

    # The value of a feature is determined once the whole macro has been parsed.
    if ')' not in line:
      return None

    # If there is exactly one string literal, that is the value. This handles
    # the 3-arg form of BASE_FEATURE.
    if len(self._string_literals) == 1:
      return self._string_literals[0]

    # If there are multiple string literals, we can't be sure which is correct.
    if len(self._string_literals) > 1:
      return None

    # If there is no string literal, this might be the 2-arg form. The value is
    # the constant name, quoted.
    if self._comma_count == 1:
      return f'"{self._constant_name}"'

    return None

  def CreateJavaConstant(self, name, value, comments):
    return java_cpp_utils.JavaString(name, value, comments)

  # This delegate requires multiline parsing to determine the feature string.
  def RequiresMultilineValueParsing(self):
    return True


def _GenerateOutput(template, source_paths, features):
  description_template = """
    // This following string constants were inserted by
    //     {SCRIPT_NAME}
    // From
    //     {SOURCE_PATHS}

"""
  values = {
      'SCRIPT_NAME': java_cpp_utils.GetScriptName(),
      'SOURCE_PATHS': ',\n    //     '.join(source_paths),
  }
  description = description_template.format(**values)
  native_features = '\n\n'.join(x.Format() for x in features)

  # TODO(agrieve): Remove {{ and }} from input templates.
  template = template.replace('{{', '{').replace('}}', '}')
  return template.replace('{NATIVE_FEATURES}', description + native_features)


def _ParseFeatureFile(path):
  with open(path, encoding='utf-8') as f:
    feature_file_parser = java_cpp_utils.CppConstantParser(
        FeatureParserDelegate(), f.readlines())
  return feature_file_parser.Parse()


def _Generate(source_paths, template):
  package, class_name = java_cpp_utils.ParseTemplateFile(template)
  output_path = java_cpp_utils.GetJavaFilePath(package, class_name)

  features = []
  for source_path in source_paths:
    features.extend(_ParseFeatureFile(source_path))

  output = _GenerateOutput(template, source_paths, features)
  return output, output_path


def _Main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--srcjar',
                      required=True,
                      help='The path at which to generate the .srcjar file')
  parser.add_argument('--template',
                      help='The template file with which to generate the Java '
                      'class. Must have "{NATIVE_FEATURES}" somewhere in '
                      'the template.')
  parser.add_argument('--class-name', help='FQN of Java class to generate')
  parser.add_argument('inputs',
                      nargs='+',
                      help='Input file(s)',
                      metavar='INPUTFILE')
  args = parser.parse_args(argv)

  if args.template:
    with open(args.template, encoding='utf-8') as f:
      template = f.read()
  else:
    template = java_cpp_utils.CreateDefaultTemplate(args.class_name,
                                                    '{NATIVE_FEATURES}')

  with action_helpers.atomic_output(args.srcjar) as f:
    with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
      data, path = _Generate(args.inputs, template)
      zip_helpers.add_to_zip_hermetic(srcjar, path, data=data)


if __name__ == '__main__':
  _Main(sys.argv[1:])
