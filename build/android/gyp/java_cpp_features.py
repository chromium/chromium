#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import zipfile

from util import build_utils
from util import java_cpp_utils


class FeatureParserDelegate(java_cpp_utils.CppConstantParser.Delegate):
  # Ex. 'const base::Feature kConstantName{"StringNameOfTheFeature", ...};'
  # would parse as:
  #   ExtractConstantName() -> 'ConstantName'
  #   ExtractValue() -> '"StringNameOfTheFeature"'
  FEATURE_RE = re.compile(r'\s*const (?:base::)?Feature\s+k(\w+)\s*(?:=\s*)?{')
  VALUE_RE = re.compile(r'\s*("(?:\"|[^"])*")\s*,')

  def ExtractConstantName(self, line):
    match = FeatureParserDelegate.FEATURE_RE.match(line)
    return match.group(1) if match else None

  def ExtractValue(self, line):
    match = FeatureParserDelegate.VALUE_RE.search(line)
    return match.group(1) if match else None

  def CreateJavaConstant(self, name, value, comments):
    return java_cpp_utils.JavaString(name, value, comments)


def _GenerateOutput(template, source_paths, template_path, features):
  description_template = """
    // This following string constants were inserted by
    //     {SCRIPT_NAME}
    // From
    //     {SOURCE_PATHS}
    // Into
    //     {TEMPLATE_PATH}

"""
  values = {
      'SCRIPT_NAME': java_cpp_utils.GetScriptName(),
      'SOURCE_PATHS': ',\n    //     '.join(source_paths),
      'TEMPLATE_PATH': template_path,
  }
  description = description_template.format(**values)
  native_features = '\n\n'.join(x.Format() for x in features)

  values = {
      'NATIVE_FEATURES': description + native_features,
  }
  return template.format(**values)


def _ParseFeatureFile(path):
  with open(path) as f:
    feature_file_parser = java_cpp_utils.CppConstantParser(
        FeatureParserDelegate(), f.readlines())
  return feature_file_parser.Parse()


def _Generate(source_paths, template_path):
  with open(template_path) as f:
    lines = f.readlines()

  template = ''.join(lines)
  package, class_name = java_cpp_utils.ParseTemplateFile(lines)
  output_path = java_cpp_utils.GetJavaFilePath(package, class_name)

  features = []
  for source_path in source_paths:
    features.extend(_ParseFeatureFile(source_path))

  output = _GenerateOutput(template, source_paths, template_path, features)
  return output, output_path


def _Main(argv):
  parser = argparse.ArgumentParser()

  parser.add_argument('--srcjar',
                      required=True,
                      help='The path at which to generate the .srcjar file')

  parser.add_argument('--template',
                      required=True,
                      help='The template file with which to generate the Java '
                      'class. Must have "{NATIVE_FEATURES}" somewhere in '
                      'the template.')

  parser.add_argument('inputs',
                      nargs='+',
                      help='Input file(s)',
                      metavar='INPUTFILE')
  args = parser.parse_args(argv)

  with build_utils.AtomicOutput(args.srcjar) as f:
    with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
      data, path = _Generate(args.inputs, args.template)
      build_utils.AddToZipHermetic(srcjar, path, data=data)


if __name__ == '__main__':
  _Main(sys.argv[1:])
